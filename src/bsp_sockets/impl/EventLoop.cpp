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
using namespace bsp_perf::shared;

void timerQueueCallback(EventLoop& loop, int fd, std::any args)
{
    std::vector<timer_event> fired_evs;
    auto& tq = loop.getTimerQueue();
    tq->get_timo(fired_evs);
    for (std::vector<timer_event>::iterator it = fired_evs.begin();
        it != fired_evs.end(); ++it)
    {
        it->cb(loop, it->cb_data);
    }
}

EventLoop::EventLoop():
    m_epoll_fd{::epoll_create1(0)},
    m_timer_que{std::make_shared<TimerQueue>()},
    m_logger{std::make_unique<BspLogger>()}
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

void EventLoop::processEvents()
{
    while (true)
    {
        int nfds = ::epoll_wait(m_epoll_fd, m_fired_events, max_events, 10);
        for (int i = 0; i < nfds; ++i)
        {
            ioevIterator it = m_io_events.find(m_fired_events[i].data.fd);
            assert(it != m_io_events.end());
            std::unique_ptr<IOEvent> ev;
            ev.reset(&(it->second));

            if (m_fired_events[i].events & EPOLLIN)
            {
                std::any args = ev->rcb_args;
                ev->read_callback(*this, m_fired_events[i].data.fd, args);
            }
            else if (m_fired_events[i].events & EPOLLOUT)
            {
                std::any args = ev->wcb_args;
                ev->write_callback(*this, m_fired_events[i].data.fd, args);
            }
            else if (m_fired_events[i].events & (EPOLLHUP | EPOLLERR))
            {
                if (ev->read_callback)
                {
                    std::any args = ev->rcb_args;
                    ev->read_callback(*this, m_fired_events[i].data.fd, args);
                }
                else if (ev->write_callback)
                {
                    std::any args = ev->wcb_args;
                    ev->write_callback(*this, m_fired_events[i].data.fd, args);
                }
                else
                {
                    m_logger->printStdoutLog(BspLogger::LogLevel::Error, "fd {} get error, delete it from epoll", m_fired_events[i].data.fd);
                    delIoEvent(m_fired_events[i].data.fd);
                }
            }
        }
        runTask();
    }
}

/*
 * if EPOLLIN in mask, EPOLLOUT must not in mask;
 * if EPOLLOUT in mask, EPOLLIN must not in mask;
 * if want to register EPOLLOUT | EPOLLIN event, just call add_ioev twice!
 */
void EventLoop::addIoEvent(int fd, ioCallback proc, int mask, std::any args)
{
    int f_mask = 0;//finial mask
    int op;
    ioevIterator it = m_io_events.find(fd);
    if (it == m_io_events.end())
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
        it->second.read_callback = proc;
        it->second.rcb_args = args;
    }
    else if (mask & EPOLLOUT)
    {
        it->second.write_callback = proc;
        it->second.wcb_args = args;
    }

    m_io_events[fd].mask = f_mask;
    struct epoll_event event;
    event.events = f_mask;
    event.data.fd = fd;
    int ret = ::epoll_ctl(m_epoll_fd, op, fd, &event);
    if (ret == -1)
    {
        throw BspSocketException("epoll_ctl");
    }
    m_listening.insert(fd);//加入到监听集合中
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

