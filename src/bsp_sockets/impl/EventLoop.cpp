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
#include <cstring>
#include <cerrno>
#include <any>

namespace bsp_sockets
{
using namespace bsp_perf::shared;

EventLoop::EventLoop():
    m_timer_queue{std::make_shared<TimerQueue>()},
    m_logger{std::make_unique<BspLogger>("EventLoop")}
{

    if (nullptr == m_timer_queue)
    {
        throw BspSocketException("new TimerQueue");
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



} //namespace bsp_sockets

