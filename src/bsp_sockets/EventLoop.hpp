#ifndef __EVENT_LOOP_H__
#define __EVENT_LOOP_H__

#include "impl/TimerQueue.hpp"
#include <shared/BspLogger.hpp>
#include <sys/epoll.h>
#include <unordered_map>
#include <unordered_set>
#include <any>
#include <functional>
#include <iostream>
#include <memory>

namespace bsp_sockets
{
static constexpr int MAX_EVENTS = 10;

using ioCallback = std::function<void(std::shared_ptr<EventLoop> loop, int fd, std::any args)>;
//让当前loop在一次poll循环后执行指定任务
using pendingFunc = std::function<void(std::shared_ptr<EventLoop>, std::any)>;

struct IOEvent//注册的IO事件
{
    int mask{0x00};             //EPOLLIN EPOLLOUT
    ioCallback read_callback{nullptr};  //callback when EPOLLIN comming
    ioCallback write_callback{nullptr}; //callback when EPOLLOUT comming
    std::any rcb_args{nullptr};   //extra arguments for read_cb
    std::any wcb_args{nullptr};  //extra arguments for write_cb
};
class EventLoop : public std::enable_shared_from_this<EventLoop>
{
public:
    EventLoop();

    void processEvents();

    //operator for IO event
    void addIoEvent(int fd, ioCallback proc, int mask, std::any args);
    //delete only mask event for fd in epoll
    void delIoEvent(int fd, int mask);
    //delete event for fd in epoll
    void delIoEvent(int fd);
    //get all fds this loop is listening
    std::unordered_set<int>& getAllListenings() { return m_listening; }

    void addTask(pendingFunc func, std::any args);
    void runTask();

    std::shared_ptr<TimerQueue>& getTimerQueue() { return m_timer_queue; }
    //operator for timer event
    int runAt(timerCallback cb, std::any args, time_t ts);
    int runAfter(timerCallback cb, std::any args, int sec, int millis = 0);
    int runEvery(timerCallback cb, std::any args, int sec, int millis = 0);
    void delTimer(int timer_id);

private:
    int m_epoll_fd{-1};
    struct epoll_event m_fired_events[MAX_EVENTS];
    //map: fd->IOEvent
    std::unordered_map<int, IOEvent> m_io_events;
    using ioevIterator = std::unordered_map<int, IOEvent>::iterator;
    std::shared_ptr<TimerQueue> m_timer_queue;
    //此队列用于:暂存将要执行的任务
    std::vector<std::pair<pendingFunc, std::any> > m_pending_factors;

    std::unordered_set<int> m_listening{};

    std::unique_ptr<bsp_perf::shared::BspLogger> m_logger;

    friend void timerQueueCallback(EventLoop& loop, int fd, std::any args);
};

}

#endif
