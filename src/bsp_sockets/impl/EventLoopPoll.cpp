#include "EventLoopPoll.hpp"
#include "BspSocketException.hpp"
#include "TimerQueue.hpp"


#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <time.h>

#include <string>
#include <memory>
#include <vector>
#include <cstring>
#include <cerrno>
#include <any>

#include <poll.h>


namespace bsp_sockets
{
using namespace bsp_perf::shared;

EventLoopPoll::EventLoopPoll():
    m_params{
        .m_timer_queue = std::make_shared<TimerQueue>(),
        .m_logger =std::make_unique<BspLogger>("EventLoopPoll")}

{
    for(int i=0; i<1024; ++i)
    {
        m_params.m_fds[i].fd = -1;
    }

    if (nullptr == m_params.m_timer_queue)
    {
        throw BspSocketException("new TimerQueue");
    }

    auto timerQueueCallback = [](std::shared_ptr<IEventLoop> loop, int fd, std::any args)
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
<<<<<<< HEAD
=======

>>>>>>> revise small issues of bsp sockets
    addIoEvent(m_params.m_timer_queue->getNotifier(), timerQueueCallback, POLLIN, m_params.m_timer_queue);
}

void EventLoopPoll::processEvents()
{
    while (true)
    {
        thread_local size_t tid = std::hash<std::thread::id>{}(std::this_thread::get_id());
        int nfds = ::poll(m_params.m_fds, m_params.m_nfds, -1);
<<<<<<< HEAD
=======

>>>>>>> revise small issues of bsp sockets
        for (int i = 0; i < m_params.m_nfds; ++i)
        {
            int fd= m_params.m_fds[i].fd;
            int events = m_params.m_fds[i].revents;
            EventLoopParams::ioevIterator it = m_params.m_io_events.find(fd);
            assert(it != m_params.m_io_events.end());
            auto& ev = it->second;

            if (events & POLLIN)
            {
                //std::cout << "@132行 in EventLoop.cpp 轮询的事件是 " << (events&POLLIN) << std::endl;
                std::any args = ev.rcb_args;
                ev.read_callback(shared_from_this(), fd, args);
            }
            else if (events & POLLOUT)
            {
                //std::cout << "@138行 in EventLoop.cpp 轮询的事件是 " << (events&POLLOUT) << std::endl;
                std::any args = ev.wcb_args;
                ev.write_callback(shared_from_this(), fd, args);
            }
            else if (events & (POLLHUP | POLLERR))
            {
                if (ev.read_callback)
                {
                    std::any args = ev.rcb_args;
                    ev.read_callback(shared_from_this(), fd, args);
                }
                else if (ev.write_callback)
                {
                    std::any args = ev.wcb_args;
                    ev.write_callback(shared_from_this(), fd, args);
                }
                else
                {
                    m_params.m_logger->printStdoutLog(BspLogger::LogLevel::Error, "fd {} get error, delete it from poll", fd);
                    //std::cout << "@157行 in EventLoop.cpp删除事件 " << fd << std::endl;
                    delIoEvent(fd);
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
void EventLoopPoll::addIoEvent(int fd, ioCallback proc, int mask, std::any args)
{

    if (mask == EPOLLIN)
    {
        mask = POLLIN;
    }
    else if (mask == EPOLLOUT)
    {
        mask = POLLOUT;
    }
<<<<<<< HEAD
=======

>>>>>>> revise small issues of bsp sockets
    int f_mask = 0;
    EventLoopParams::ioevIterator it = m_params.m_io_events.find(fd);
    if (it == m_params.m_io_events.end())
    {
        //std::cout<< this << "添加第" << ++add_event_times << "个事件" << fd <<"  "<< (mask&POLLIN)  <<std::endl;
        f_mask = mask;
    }
    else
    {
        //std::cout<< this << "@230行 in EventLoop.cpp 修改现有事件" << fd <<std::endl;
        f_mask = it->second.mask | mask;
    }

    if (mask & POLLIN)
    {
        m_params.m_io_events[fd].read_callback = proc;
        m_params.m_io_events[fd].rcb_args = args;
    }
    else if (mask & POLLOUT)
    {
        m_params.m_io_events[fd].write_callback = proc;
        m_params.m_io_events[fd].wcb_args = args;
    }

    m_params.m_io_events[fd].mask = f_mask;

    if (it == m_params.m_io_events.end())
    {
        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = f_mask;
        m_params.m_fds[m_params.m_nfds++] = pfd;
        //std::cout << this << "   "<< m_nfds << " @251行 in EventLoop.cpp" <<std::endl;
    }
    else
    {
        for (int i = 0; i < m_params.m_nfds; i++)
        {
            if (m_params.m_fds[i].fd == fd)
            {
                m_params.m_fds[i].events = f_mask;
                break;
            }
        }
    }
    m_params.m_listening.insert(fd); //加入到监听集合中
}

void EventLoopPoll::delIoEvent(int fd, int mask)
{
    EventLoopParams::ioevIterator it = m_params.m_io_events.find(fd);
    if (it == m_params.m_io_events.end())
    {
        return;
    }
    int& o_mask = it->second.mask;
    if (mask == EPOLLIN)
    {
        mask = POLLIN;
    }
    else if (mask == EPOLLOUT)
    {
        mask = POLLOUT;
    }
    int ret;
    o_mask = o_mask & (~mask);
    if (o_mask == 0)
    {
        //std::cout << "@327 in EventLoop.cpp 删除事件" << fd <<std::endl;
        m_params.m_io_events.erase(it);
        for (int i = 0; i < m_params.m_nfds; ++i)
        {
            if (m_params.m_fds[i].fd == fd)
            {
                // Shift the remaining elements to the left
                for (int j = i; j < m_params.m_nfds - 1; ++j)
                {
                    m_params.m_fds[j] = m_params.m_fds[j + 1];
                }
                --m_params.m_nfds;
                break;
            }
        }
        m_params.m_listening.erase(fd);
    }
    else
    {
        //std::cout << "@346 in EventLoop.cpp修改事件" << fd <<std::endl;
        for (int i = 0; i < m_params.m_nfds; ++i)
        {
            if (m_params.m_fds[i].fd == fd)
            {
                m_params.m_fds[i].events = o_mask;
                break;
            }
        }
    }
}

void EventLoopPoll::delIoEvent(int fd)
{
    EventLoopParams::ioevIterator it = m_params.m_io_events.find(fd);
    if (it == m_params.m_io_events.end())
    {
        return;
    }
    m_params.m_io_events.erase(it);
    m_params.m_listening.erase(fd);

    for (int i = 0; i < m_params.m_nfds; i++)
    {
        if (m_params.m_fds[i].fd == fd)
        {
            for (int j = i; j < m_params.m_nfds - 1; j++)
            {
                m_params.m_fds[j] = m_params.m_fds[j + 1];
            }
            --m_params.m_nfds;
            break;
        }
    }

}

std::unordered_set<int>& EventLoopPoll::getAllListenings()
{
    return m_params.m_listening;
}

void EventLoopPoll::addTask(pendingFunc func, std::any args)
{
    std::pair<pendingFunc, std::any> item(func, args);
    m_params.m_pending_factors.push_back(item);
}


void EventLoopPoll::runTask()
{
    std::vector<std::pair<pendingFunc, std::any> >::iterator it;
    for (it = m_params.m_pending_factors.begin(); it != m_params.m_pending_factors.end(); ++it)
    {
        pendingFunc func = it->first;
        std::any args = it->second;
        func(shared_from_this(), args);
    }
    m_params.m_pending_factors.clear();
}

int EventLoopPoll::runAt(timerCallback cb, std::any args, time_t ts)
{
    timerEvent te(cb, args, ts);
    return m_params.m_timer_queue->addTimer(te);
}

int EventLoopPoll::runAfter(timerCallback cb, std::any args, int sec, int millis)
{
    struct timespec tpc;
    clock_gettime(CLOCK_REALTIME, &tpc);
    uint64_t ts = tpc.tv_sec * 1000 + tpc.tv_nsec / 1000000UL;
    ts += sec * 1000 + millis;
    runAt(cb, args, ts);
}

int EventLoopPoll::runEvery(timerCallback cb, std::any args, int sec, int millis)
{
    uint32_t interval = sec * 1000 + millis;
    struct timespec tpc;
    clock_gettime(CLOCK_REALTIME, &tpc);
    uint64_t ts = tpc.tv_sec * 1000 + tpc.tv_nsec / 1000000UL + interval;
    timerEvent te(cb, args, ts, interval);
    return m_params.m_timer_queue->addTimer(te);
}

void EventLoopPoll::delTimer(int timer_id)
{
    m_params.m_timer_queue->delTimer(timer_id);
}


std::shared_ptr<TimerQueue>& EventLoopPoll::getTimerQueue()
{
    return m_params.m_timer_queue;
}

} //namespace bsp_sockets

