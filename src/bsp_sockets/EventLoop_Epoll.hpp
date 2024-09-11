#ifndef __EVENT_LOOP_EPOLL_H__
#define __EVENT_LOOP_EPOLL_H__

#include "EventLoop.hpp"


namespace bsp_sockets
{

class EventLoop_Epoll: public EventLoop, public std::enable_shared_from_this<EventLoop_Epoll>
{
public:
    EventLoop_Epoll();

    void processEvents() override;

    //operator for IO event
    void addIoEvent(int fd, ioCallback proc, int mask, std::any args) override;
    //delete only mask event for fd in epoll
    void delIoEvent(int fd, int mask) override;
    //delete event for fd in epoll
    void delIoEvent(int fd) override;

    std::unordered_set<int>& getAllListenings() override;

    void addTask(pendingFunc func, std::any args) override;

    void runTask() override;

    int runAt(timerCallback cb, std::any args, time_t ts) override;
    int runAfter(timerCallback cb, std::any args, int sec, int millis = 0) override;
    int runEvery(timerCallback cb, std::any args, int sec, int millis = 0) override;
    void delTimer(int timer_id) override;

    std::shared_ptr<TimerQueue>& getTimerQueue() override;


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