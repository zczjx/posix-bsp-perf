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



static void timerQueueCallback(evutil_socket_t fd, short what, void *arg)
{
    auto* loop = static_cast<EventLoopLibevent*>(arg);
    std::vector<timerEvent> fired_evs;
    auto tq = loop->m_params.m_timer_queue;
    tq->getFiredTimerEvents(fired_evs);
    for (auto& event : fired_evs)
    {
        event.cb(loop->shared_from_this(), event.cb_data);
    }
}

static void lib_callback(evutil_socket_t fd_t, short what, void *arg)
{
    //std::cout << "39行 in  EventLoopLibevent.cpp" << std::endl;
    int fd = static_cast<int>(fd_t);
    EventLoopLibevent* loop = static_cast<EventLoopLibevent*>(arg);
    if (!loop) {
        std::cerr << "Error: loop is null" << std::endl;
        return;
    }

    if (what == EV_READ)
    {
        std::any rcbarg = loop->m_params.m_io_events[fd].rcb_args;
        if (loop->m_params.m_io_events[fd].read_callback) {
            loop->m_params.m_io_events[fd].read_callback(loop->shared_from_this(), fd, rcbarg);
        } else {
            std::cerr << "Error: read_callback is not set for fd: " << fd << std::endl;
        }
    }
    else if (what == EV_WRITE)
    {
        std::any wcbarg = loop->m_params.m_io_events[fd].wcb_args;
        if (loop->m_params.m_io_events[fd].write_callback) {
            loop->m_params.m_io_events[fd].write_callback(loop->shared_from_this(), fd, wcbarg);
        } else {
            std::cerr << "Error: write_callback is not set for fd: " << fd << std::endl;
        }
    }
    else if (what == EV_SIGNAL)
    {
        if (loop->m_params.m_io_events[fd].read_callback)
        {
            std::any rcbarg = loop->m_params.m_io_events[fd].rcb_args;
            loop->m_params.m_io_events[fd].read_callback(loop->shared_from_this(), fd, rcbarg);
        }
        else if (loop->m_params.m_io_events[fd].write_callback)
        {
            std::any wcbarg = loop->m_params.m_io_events[fd].wcb_args;
            loop->m_params.m_io_events[fd].write_callback(loop->shared_from_this(), fd, wcbarg);
        }
    }
}



EventLoopLibevent::EventLoopLibevent():
    m_params{
    .m_timer_queue = std::make_shared<TimerQueue>(),
    .m_logger =std::make_unique<BspLogger>("EventLoopLibevent")},
    m_event_base{event_base_new()}

{
    if (nullptr == m_event_base)
    {
        throw BspSocketException("event_base_new");
    }

    if (nullptr == m_params.m_timer_queue)
    {
        throw BspSocketException("new TimerQueue");
    }

    struct event *timer_event = event_new(m_event_base, m_params.m_timer_queue->getNotifier(), EV_READ | EV_PERSIST, timerQueueCallback, this);

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
        if (event_base_loop(m_event_base, EVLOOP_ONCE) < 0)
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
    int f_mask = 0;
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

    short events = 0;
    if(mask & EPOLLIN)
    {
        events |= EV_READ;
        m_params.m_io_events[fd].read_callback = proc;
        m_params.m_io_events[fd].rcb_args = args;
    }
    else if (mask & EPOLLOUT)
    {
        events |= EV_WRITE;
        m_params.m_io_events[fd].write_callback = proc;
        m_params.m_io_events[fd].wcb_args = args;
    }

    //auto *params = new std::tuple<std::function<void(std::shared_ptr<IEventLoop>, int, std::any)>, std::shared_ptr<IEventLoop>, std::any>(proc, shared_from_this(), args);
    struct event *io_event = event_new(m_event_base, fd, events | EV_PERSIST,lib_callback, this);

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
        m_libevent_io_events[fd].event_for_read = io_event;
    }
    else if (mask & EPOLLOUT)
    {
        m_libevent_io_events[fd].event_for_write = io_event;
    }

}

void EventLoopLibevent::delIoEvent(int fd, int mask)
{
    EventLoopParams::ioevIterator it = m_params.m_io_events.find(fd);
    auto it_libevent = m_libevent_io_events.find(fd);
    if (it == m_params.m_io_events.end())
    {
        return;
    }
    
    short events = 0;
    std::any cb_args;

    int& o_mask = it->second.mask;
    o_mask = o_mask & (~mask);
    if (o_mask == 0)
    {
        // Delete the event
        event_del(it_libevent->second.event_for_read);
        event_free(it_libevent->second.event_for_read);
        event_del(it_libevent->second.event_for_write);
        event_free(it_libevent->second.event_for_write);
        m_params.m_io_events.erase(it);
        m_params.m_listening.erase(fd); // Remove from listening set
    }
    else
    {
        // Modify the event
        if (o_mask & EPOLLIN)
        {
            event_del(it_libevent->second.event_for_write);
            event_free(it_libevent->second.event_for_write);

        }
        if (o_mask & EPOLLOUT)
        {
            event_del(it_libevent->second.event_for_read);
            event_free(it_libevent->second.event_for_read);
        }
    }
}

void EventLoopLibevent::delIoEvent(int fd)
{
    EventLoopParams::ioevIterator it = m_params.m_io_events.find(fd);
    auto it_libevent = m_libevent_io_events.find(fd);
    if (it == m_params.m_io_events.end())
    {
        return;
    }
    // Delete the event
    if (it_libevent->second.event_for_read)
    {
        event_del(it_libevent->second.event_for_read);
        event_free(it_libevent->second.event_for_read);
    }
    else if (it_libevent->second.event_for_write)
    {
        event_del(it_libevent->second.event_for_write);
        event_free(it_libevent->second.event_for_write);
    }
    
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

