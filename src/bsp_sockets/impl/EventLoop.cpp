#include <bsp_sockets/EventLoop.hpp>
#include "BspSocketException.hpp"
#include "TimerQueue.hpp"

#include <sys/epoll.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <time.h>

#include <string>
#include <memory>
#include <vector>
#include <any>

namespace bsp_sockets
{
using namespace bsp_perf::shared;

EventLoop::EventLoop():
    m_epoll_fd{::epoll_create1(0)},
    m_timer_queue{std::make_shared<TimerQueue>()},
    m_logger{std::make_unique<BspLogger>("EventLoop")}
{
    if (m_epoll_fd < 0)
    {
        throw BspSocketException("epoll_create1");
    }

    if (nullptr == m_timer_queue)
    {
        throw BspSocketException("new TimerQueue");
    }

    auto timerQueueCallback = [](std::shared_ptr<EventLoop> loop, int fd, std::any args)
    {
        std::vector<timerEvent> fired_evs;
        auto& tq = loop->getTimerQueue();
        tq->getFiredTimerEvents(fired_evs);
        for (std::vector<timerEvent>::iterator it = fired_evs.begin();
            it != fired_evs.end(); ++it)
        {
            it->cb(loop, it->cb_data);
        }
    };

    addIoEvent(m_timer_queue->getNotifier(), timerQueueCallback, EPOLLIN, m_timer_queue);
}

void EventLoop::processEvents()
{
    while (true)
    {
        thread_local size_t tid = std::hash<std::thread::id>{}(std::this_thread::get_id());
        int nfds = ::epoll_wait(m_epoll_fd, m_fired_events, MAX_EVENTS, 10);
        for (int i = 0; i < nfds; ++i)
        {
            ioevIterator it = m_io_events.find(m_fired_events[i].data.fd);
            assert(it != m_io_events.end());
            auto& ev = it->second;

            if (m_fired_events[i].events & EPOLLIN)
            {
                std::cout << "[S] zczjx--> processEvents read_callback i: " << i << std::endl;
                std::any args = ev.rcb_args;
                ev.read_callback(shared_from_this(), m_fired_events[i].data.fd, args);
                std::cout << "[E] zczjx--> processEvents read_callback i: " << i << std::endl;
            }
            else if (m_fired_events[i].events & EPOLLOUT)
            {
                std::cout << "[S] zczjx--> processEvents write_callback" << std::endl;
                std::any args = ev.wcb_args;
                ev.write_callback(shared_from_this(), m_fired_events[i].data.fd, args);
                std::cout << "[E] zczjx--> processEvents write_callback" << std::endl;
            }
            else if (m_fired_events[i].events & (EPOLLHUP | EPOLLERR))
            {
                std::cout << "[S] zczjx--> processEvents EPOLLHUP | EPOLLERR" << std::endl;
                if (ev.read_callback)
                {
                    std::any args = ev.rcb_args;
                    ev.read_callback(shared_from_this(), m_fired_events[i].data.fd, args);
                }
                else if (ev.write_callback)
                {
                    std::any args = ev.wcb_args;
                    ev.write_callback(shared_from_this(), m_fired_events[i].data.fd, args);
                }
                else
                {
                    m_logger->printStdoutLog(BspLogger::LogLevel::Error, "fd {} get error, delete it from epoll", m_fired_events[i].data.fd);
                    delIoEvent(m_fired_events[i].data.fd);
                }
                std::cout << "[E] zczjx--> processEvents EPOLLHUP | EPOLLERR" << std::endl;
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
    int f_mask = 0;
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
        m_io_events[fd].read_callback = proc;
        m_io_events[fd].rcb_args = args;
    }
    else if (mask & EPOLLOUT)
    {
        m_io_events[fd].write_callback = proc;
        m_io_events[fd].wcb_args = args;
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

void EventLoop::delIoEvent(int fd, int mask)
{
    ioevIterator it = m_io_events.find(fd);
    if (it == m_io_events.end())
    {
        return ;
    }
    int& o_mask = it->second.mask;
    int ret;
    o_mask = o_mask & (~mask);
    if (o_mask == 0)
    {
        m_io_events.erase(it);
        ret = ::epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, fd, NULL);
        if (ret == -1)
        {
            throw BspSocketException("epoll_ctl EPOLL_CTL_DEL");
        }
        m_listening.erase(fd);//从监听集合中删除
    }
    else
    {
        struct epoll_event event;
        event.events = o_mask;
        event.data.fd = fd;
        ret = ::epoll_ctl(m_epoll_fd, EPOLL_CTL_MOD, fd, &event);
        if (ret == -1)
        {
            throw BspSocketException("epoll_ctl EPOLL_CTL_MOD");
        }
    }
}

void EventLoop::delIoEvent(int fd)
{
    ioevIterator it = m_io_events.find(fd);
    if (it == m_io_events.end())
    {
        return;
    }
    m_io_events.erase(it);
    m_listening.erase(fd);
    int ret = ::epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    if (ret == -1)
    {
        throw BspSocketException("epoll_ctl EPOLL_CTL_DEL");
    }
}

int EventLoop::runAt(timerCallback cb, std::any args, time_t ts)
{
    timerEvent te(cb, args, ts);
    return m_timer_queue->addTimer(te);
}

int EventLoop::runAfter(timerCallback cb, std::any args, int sec, int millis)
{
    struct timespec tpc;
    clock_gettime(CLOCK_REALTIME, &tpc);
    uint64_t ts = tpc.tv_sec * 1000 + tpc.tv_nsec / 1000000UL;
    ts += sec * 1000 + millis;
    runAt(cb, args, ts);
}

int EventLoop::runEvery(timerCallback cb, std::any args, int sec, int millis)
{
    uint32_t interval = sec * 1000 + millis;
    struct timespec tpc;
    clock_gettime(CLOCK_REALTIME, &tpc);
    uint64_t ts = tpc.tv_sec * 1000 + tpc.tv_nsec / 1000000UL + interval;
    timerEvent te(cb, args, ts, interval);
    return m_timer_queue->addTimer(te);
}

void EventLoop::delTimer(int timer_id)
{
    m_timer_queue->delTimer(timer_id);
}

void EventLoop::addTask(pendingFunc func, std::any args)
{
    std::pair<pendingFunc, std::any> item(func, args);
    m_pending_factors.push_back(item);
}

void EventLoop::runTask()
{
    std::vector<std::pair<pendingFunc, std::any> >::iterator it;
    for (it = m_pending_factors.begin(); it != m_pending_factors.end(); ++it)
    {
        pendingFunc func = it->first;
        std::any args = it->second;
        func(shared_from_this(), args);
    }
    m_pending_factors.clear();
}

} //namespace bsp_sockets

