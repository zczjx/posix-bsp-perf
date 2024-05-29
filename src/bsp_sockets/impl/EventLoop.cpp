#include <bsp_sockets/EventLoop.hpp>
#include <bsp_sockets/BspSocketException.hpp>
#include "TimerQueue.hpp"

#include <sys/epoll.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <time.h>

namespace bsp_sockets
{
void timerQueueCallback(EventLoop& loop, int fd, std::any args)
{
    std::vector<timer_event> fired_evs;
    loop.m_timer_que->get_timo(fired_evs);
    for (std::vector<timer_event>::iterator it = fired_evs.begin();
        it != fired_evs.end(); ++it)
    {
        it->cb(loop, it->cb_data);
    }
}

EventLoop::EventLoop():
    m_epoll_fd{::epoll_create1(0)},
    m_timer_que{std::make_shared<TimerQueue>()}
{

    if (m_epoll_fd = -1)
    {
        throw BspSocketException("epoll_create1");
    }

    if (m_timer_que == nullptr)
    {
        throw BspSocketException("new TimerQueue");
    }

    addIoEvent(m_timer_que->notifier(), timerQueueCallback, EPOLLIN, m_timer_que);
}

void EventLoop::process_evs()
{
    while (true)
    {
        //handle file event
        ioev_it it;
        int nfds = ::epoll_wait(_epfd, _fired_evs, MAXEVENTS, 10);
        for (int i = 0;i < nfds; ++i)
        {
            it = _io_evs.find(_fired_evs[i].data.fd);
            assert(it != _io_evs.end());
            io_event* ev = &(it->second);

            if (_fired_evs[i].events & EPOLLIN)
            {
                void *args = ev->rcb_args;
                ev->read_cb(this, _fired_evs[i].data.fd, args);
            }
            else if (_fired_evs[i].events & EPOLLOUT)
            {
                void *args = ev->wcb_args;
                ev->write_cb(this, _fired_evs[i].data.fd, args);
            }
            else if (_fired_evs[i].events & (EPOLLHUP | EPOLLERR))
            {
                if (ev->read_cb)
                {
                    void *args = ev->rcb_args;
                    ev->read_cb(this, _fired_evs[i].data.fd, args);
                }
                else if (ev->write_cb)
                {
                    void *args = ev->wcb_args;
                    ev->write_cb(this, _fired_evs[i].data.fd, args);
                }
                else
                {
                    error_log("fd %d get error, delete it from epoll", _fired_evs[i].data.fd);
                    del_ioev(_fired_evs[i].data.fd);
                }
            }
        }
        run_task();
    }
}

/*
 * if EPOLLIN in mask, EPOLLOUT must not in mask;
 * if EPOLLOUT in mask, EPOLLIN must not in mask;
 * if want to register EPOLLOUT | EPOLLIN event, just call add_ioev twice!
 */
void EventLoop::add_ioev(int fd, io_callback* proc, int mask, void* args)
{
    int f_mask = 0;//finial mask
    int op;
    ioev_it it = _io_evs.find(fd);
    if (it == _io_evs.end())
    {
        f_mask = mask;
        op = EPOLL_CTL_ADD;
    }
    else
    {
        f_mask = it->second.mask | mask;
        op = EPOLL_CTL_MOD;
    }
    if (mask & EPOLLIN)
    {
        _io_evs[fd].read_cb = proc;
        _io_evs[fd].rcb_args = args;
    }
    else if (mask & EPOLLOUT)
    {
        _io_evs[fd].write_cb = proc;
        _io_evs[fd].wcb_args = args;
    }

    _io_evs[fd].mask = f_mask;
    struct epoll_event event;
    event.events = f_mask;
    event.data.fd = fd;
    int ret = ::epoll_ctl(_epfd, op, fd, &event);
    error_if(ret == -1, "epoll_ctl");
    listening.insert(fd);//加入到监听集合中
}

void EventLoop::del_ioev(int fd, int mask)
{
    ioev_it it = _io_evs.find(fd);
    if (it == _io_evs.end())
        return ;
    int& o_mask = it->second.mask;
    int ret;
    //remove mask from o_mask
    o_mask = o_mask & (~mask);
    if (o_mask == 0)
    {
        _io_evs.erase(it);
        ret = ::epoll_ctl(_epfd, EPOLL_CTL_DEL, fd, NULL);
        error_if(ret == -1, "epoll_ctl EPOLL_CTL_DEL");
        listening.erase(fd);//从监听集合中删除
    }
    else
    {
        struct epoll_event event;
        event.events = o_mask;
        event.data.fd = fd;
        ret = ::epoll_ctl(_epfd, EPOLL_CTL_MOD, fd, &event);
        error_if(ret == -1, "epoll_ctl EPOLL_CTL_MOD");
    }
}

void EventLoop::del_ioev(int fd)
{
    _io_evs.erase(fd);
    listening.erase(fd);//从监听集合中删除
    ::epoll_ctl(_epfd, EPOLL_CTL_DEL, fd, NULL);
}

int EventLoop::run_at(timer_callback cb, void* args, uint64_t ts)
{
    timer_event te(cb, args, ts);
    return _timer_que->add_timer(te);
}

int EventLoop::run_after(timer_callback cb, void* args, int sec, int millis)
{
    struct timespec tpc;
    clock_gettime(CLOCK_REALTIME, &tpc);
    uint64_t ts = tpc.tv_sec * 1000 + tpc.tv_nsec / 1000000UL;
    ts += sec * 1000 + millis;
    timer_event te(cb, args, ts);
    return _timer_que->add_timer(te);
}

int EventLoop::run_every(timer_callback cb, void* args, int sec, int millis)
{
    uint32_t interval = sec * 1000 + millis;
    struct timespec tpc;
    clock_gettime(CLOCK_REALTIME, &tpc);
    uint64_t ts = tpc.tv_sec * 1000 + tpc.tv_nsec / 1000000UL + interval;
    timer_event te(cb, args, ts, interval);
    return _timer_que->add_timer(te);
}

void EventLoop::del_timer(int timer_id)
{
    _timer_que->del_timer(timer_id);
}

void EventLoop::add_task(pendingFunc func, void* args)
{
    std::pair<pendingFunc, void*> item(func, args);
    _pendingFactors.push_back(item);
}

void EventLoop::run_task()
{
    std::vector<std::pair<pendingFunc, void*> >::iterator it;
    for (it = _pendingFactors.begin();it != _pendingFactors.end(); ++it)
    {
        pendingFunc func = it->first;
        void* args = it->second;
        func(this, args);
    }
    _pendingFactors.clear();
}


}

