#include "EventLoopLibevent.hpp"
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
#include <cstring>
#include <cerrno>
#include <any>

namespace bsp_sockets
{
using namespace bsp_perf::shared;

EventLoopLibevent::EventLoopLibevent():
    m_params{
    .m_event_base = event_base_new(),
    .m_timer_queue = std::make_shared<TimerQueue>(),
    .m_logger =std::make_unique<BspLogger>("EventLoopLibevent")}

{
    std::cout << "libevent 666666666666666666666666666666666666666" << std::endl;
    if (nullptr == m_params.m_event_base)
    {
        throw BspSocketException("event_base_new");
    }

    if (nullptr == m_params.m_timer_queue)
    {
        throw BspSocketException("new TimerQueue");
    }
    std::cout << "40行 in  EventLoopLibevent.cpp" <<std::endl;
    /*
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

    addIoEvent(m_params.m_timer_queue->getNotifier(), timerQueueCallback, EPOLLIN, m_params.m_timer_queue);
    */

    auto timerQueueCallback = [](evutil_socket_t fd, short what, void *arg)
    {
        auto loop_ptr = static_cast<std::shared_ptr<IEventLoop>*>(arg);
        std::shared_ptr<IEventLoop> loop = *loop_ptr;
        std::vector<timerEvent> fired_evs;
        auto& tq = loop->getTimerQueue();
        tq->getFiredTimerEvents(fired_evs);
        for (auto& event : fired_evs)
        {
            event.cb(loop, event.cb_data);
        }
    };

    auto * params = new std::shared_ptr<IEventLoop>(shared_from_this());
    std::cout << "71行 in  EventLoopLibevent.cpp" <<std::endl;
    struct event *timer_event = event_new(m_params.m_event_base, m_params.m_timer_queue->getNotifier(), EV_READ | EV_PERSIST, timerQueueCallback, params);
    m_params.m_io_events[m_params.m_timer_queue->getNotifier()].libread_callback = timerQueueCallback;
    m_params.m_io_events[m_params.m_timer_queue->getNotifier()].mask = EPOLLIN;
    m_params.m_io_events[m_params.m_timer_queue->getNotifier()].event_for_read = timer_event;
    m_params.m_io_events[m_params.m_timer_queue->getNotifier()].rcb_args = params;
    

    if (nullptr == timer_event)
    {
        throw BspSocketException("event_new");
    }

    if (event_add(timer_event, nullptr) < 0)
    {
        throw BspSocketException("event_add");
    }
}

void EventLoopLibevent::processEvents()
{
    while (true)
    {
        thread_local size_t tid = std::hash<std::thread::id>{}(std::this_thread::get_id());
        if (event_base_loop(m_params.m_event_base, EVLOOP_ONCE) < 0)
        {
            throw BspSocketException("event_base_loop");
        }
        runTask();
    }
}

/*
 * if EPOLLIN in mask, EPOLLOUT must not in mask;
 * if EPOLLOUT in mask, EPOLLIN must not in mask;
 * if want to register EPOLLOUT | EPOLLIN event, just call add_ioev twice!
 */

void EventLoopLibevent::addIoEvent(int fd, ioCallback proc, int mask, std::any args)
{
    /*
    int f_mask = 0;
    int op;
    EventLoopParams::ioevIterator it = m_params.m_io_events.find(fd);
    if (it == m_params.m_io_events.end())
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
        m_params.m_io_events[fd].read_callback = proc;
        m_params.m_io_events[fd].rcb_args = args;
    }
    else if (mask & EPOLLOUT)
    {
        m_params.m_io_events[fd].write_callback = proc;
        m_params.m_io_events[fd].wcb_args = args;
    }

    m_params.m_io_events[fd].mask = f_mask;
    struct epoll_event event;
    event.events = f_mask;
    event.data.fd = fd;
    int ret = ::epoll_ctl(m_params.m_epoll_fd, op, fd, &event);
    if (ret == -1)
    {
        int errnum = errno;
        m_params.m_logger->printStdoutLog(BspLogger::LogLevel::Error, "EventLoop::addIoEvent epoll_ctl ret:{}, m_epoll_fd:{}, fd:{}, Error num: {}",
                    ret, m_params.m_epoll_fd, fd, strerror(errnum));
        throw BspSocketException("epoll_ctl");
    }
    m_params.m_listening.insert(fd); //加入到监听集合中
    */

    int f_mask = 0;
    int op;
    EventLoopParams::ioevIterator it = m_params.m_io_events.find(fd);
    if (it == m_params.m_io_events.end())
    {
        f_mask = mask;
    }
    else
    {
        f_mask = it->second.mask | mask;
    }
    
    m_params.m_io_events[fd].mask = f_mask;

    short events;

    auto lib_Callback = [](evutil_socket_t fd, short what, void *arg)
    {
        std::cout << "167行 in  EventLoopLibevent.cpp" <<std::endl;
        auto *params = static_cast<std::tuple<std::function<void(std::shared_ptr<IEventLoop>, int, std::any)>, std::shared_ptr<IEventLoop>, std::any>*>(arg);
        auto &cb = std::get<0>(*params);
        auto &loop = std::get<1>(*params);
        auto &args = std::get<2>(*params);
        cb(loop, fd, args);
    };

    auto *params = new std::tuple<std::function<void(std::shared_ptr<IEventLoop>, int, std::any)>, std::shared_ptr<IEventLoop>, std::any>(proc, shared_from_this(), args);
    struct event *io_event = event_new(m_params.m_event_base, fd, events | EV_PERSIST,lib_Callback, params);

    if (nullptr == io_event)
    {
        throw BspSocketException("event_new");
    }

    if (event_add(io_event, nullptr) < 0)
    {
        throw BspSocketException("event_add");
    }

    if (mask & EPOLLIN)
    {
        m_params.m_io_events[fd].libread_callback = lib_Callback;
        m_params.m_io_events[fd].rcb_args = params;
        m_params.m_io_events[fd].event_for_read = io_event;
    }
    else if (mask & EPOLLOUT)
    {
        m_params.m_io_events[fd].libwrite_callback = lib_Callback;
        m_params.m_io_events[fd].wcb_args = params;
        m_params.m_io_events[fd].event_for_write = io_event;
    }

}

void EventLoopLibevent::delIoEvent(int fd, int mask)
{
    /*
    EventLoopParams::ioevIterator it = m_params.m_io_events.find(fd);
    if (it == m_params.m_io_events.end())
    {
        return ;
    }
    int& o_mask = it->second.mask;
    int ret;
    o_mask = o_mask & (~mask);
    if (o_mask == 0)
    {
        m_params.m_io_events.erase(it);
        ret = ::epoll_ctl(m_params.m_epoll_fd, EPOLL_CTL_DEL, fd, NULL);
        if (ret == -1)
        {
        int errnum = errno;
        m_params.m_logger->printStdoutLog(BspLogger::LogLevel::Error, "EventLoop::delIoEvent EPOLL_CTL_DEL ret:{}, m_epoll_fd:{}, fd:{}, Error num: {}",
                    ret, m_params.m_epoll_fd, fd, strerror(errnum));
            throw BspSocketException("epoll_ctl EPOLL_CTL_DEL");
        }
        m_params.m_listening.erase(fd);//从监听集合中删除
    }
    else
    {
        struct epoll_event event;
        event.events = o_mask;
        event.data.fd = fd;
        ret = ::epoll_ctl(m_params.m_epoll_fd, EPOLL_CTL_MOD, fd, &event);
        if (ret == -1)
        {
            int errnum = errno;
            m_params.m_logger->printStdoutLog(BspLogger::LogLevel::Error, "EventLoop::delIoEvent EPOLL_CTL_MOD ret:{}, m_epoll_fd:{}, fd:{}, Error num: {}",
                        ret, m_params.m_epoll_fd, fd, strerror(errnum));
            throw BspSocketException("epoll_ctl EPOLL_CTL_MOD");
        }
    }
    */

    EventLoopParams::ioevIterator it = m_params.m_io_events.find(fd);
    if (it == m_params.m_io_events.end())
    {
        return;
    }
    
    short events = 0;
    libCallback cb;
    std::any cb_args;


    int& o_mask = it->second.mask;
    o_mask = o_mask & (~mask);
    if (o_mask == 0)
    {
        // Delete the event
        event_del(it->second.event_for_read);
        event_free(it->second.event_for_read);
        event_del(it->second.event_for_write);
        event_free(it->second.event_for_write);
        m_params.m_io_events.erase(it);
        m_params.m_listening.erase(fd); // Remove from listening set
    }
    else
    {
        // Modify the event
        if (o_mask & EPOLLIN)
        {
            events = EV_READ;
            cb = it->second.libread_callback;
            cb_args = it->second.rcb_args;
            event_del(it->second.event_for_write);
            event_free(it->second.event_for_write);

        }
        if (o_mask & EPOLLOUT)
        {
            events = EV_WRITE;
            cb = it->second.libwrite_callback;
            cb_args = it->second.wcb_args;
            event_del(it->second.event_for_read);
            event_free(it->second.event_for_read);
        }
    }
}

void EventLoopLibevent::delIoEvent(int fd)
{
    /*
    EventLoopParams::ioevIterator it = m_params.m_io_events.find(fd);
    if (it == m_params.m_io_events.end())
    {
        return;
    }
    m_params.m_io_events.erase(it);
    m_params.m_listening.erase(fd);
    int ret = ::epoll_ctl(m_params.m_epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    if (ret == -1)
    {
        int errnum = errno;
        m_params.m_logger->printStdoutLog(BspLogger::LogLevel::Error, "EventLoop::delIoEvent EPOLL_CTL_DEL ret:{}, m_epoll_fd:{}, fd:{}, Error num: {}",
                    ret, m_params.m_epoll_fd, fd, strerror(errnum));
        throw BspSocketException("epoll_ctl EPOLL_CTL_DEL");
    }
    */

    EventLoopParams::ioevIterator it = m_params.m_io_events.find(fd);
    if (it == m_params.m_io_events.end())
    {
        return;
    }

    // Delete the event
    if (event_del(it->second.event_for_read) == -1)
    {
        int errnum = errno;
        m_params.m_logger->printStdoutLog(BspLogger::LogLevel::Error, "EventLoop::delIoEvent event_del failed for fd: {}, Error num: {}", fd, strerror(errnum));
        throw BspSocketException("event_del");
    }
    if (event_del(it->second.event_for_write) == -1)
    {
        int errnum = errno;
        m_params.m_logger->printStdoutLog(BspLogger::LogLevel::Error, "EventLoop::delIoEvent event_del failed for fd: {}, Error num: {}", fd, strerror(errnum));
        throw BspSocketException("event_del");
    }

    event_free(it->second.event_for_read);
    event_free(it->second.event_for_write);
    m_params.m_io_events.erase(it);
    m_params.m_listening.erase(fd);

}

std::unordered_set<int>& EventLoopLibevent::getAllListenings()
{
    return m_params.m_listening;
}

void EventLoopLibevent::addTask(pendingFunc func, std::any args)
{
    std::pair<pendingFunc, std::any> item(func, args);
    m_params.m_pending_factors.push_back(item);
}

void EventLoopLibevent::runTask()
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

int EventLoopLibevent::runAt(timerCallback cb, std::any args, time_t ts)
{
    timerEvent te(cb, args, ts);
    return m_params.m_timer_queue->addTimer(te);
}

int EventLoopLibevent::runAfter(timerCallback cb, std::any args, int sec, int millis)
{
    struct timespec tpc;
    clock_gettime(CLOCK_REALTIME, &tpc);
    uint64_t ts = tpc.tv_sec * 1000 + tpc.tv_nsec / 1000000UL;
    ts += sec * 1000 + millis;
    runAt(cb, args, ts);
}

int EventLoopLibevent::runEvery(timerCallback cb, std::any args, int sec, int millis)
{
    uint32_t interval = sec * 1000 + millis;
    struct timespec tpc;
    clock_gettime(CLOCK_REALTIME, &tpc);
    uint64_t ts = tpc.tv_sec * 1000 + tpc.tv_nsec / 1000000UL + interval;
    timerEvent te(cb, args, ts, interval);
    return m_params.m_timer_queue->addTimer(te);
}

void EventLoopLibevent::delTimer(int timer_id)
{
    m_params.m_timer_queue->delTimer(timer_id);
}

std::shared_ptr<TimerQueue>& EventLoopLibevent::getTimerQueue()
{
    return m_params.m_timer_queue;
}

} //namespace bsp_sockets

