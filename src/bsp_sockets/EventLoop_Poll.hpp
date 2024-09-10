#ifndef __EVENT_LOOP_POLL_H__
#define __EVENT_LOOP_POLL_H__


#include <poll.h>
#include "EventLoop.hpp"

//#define USE_EPOLL

namespace bsp_sockets
{

class EventLoop_Poll : public EventLoop, public std::enable_shared_from_this<EventLoop_Poll>
{
public:
    EventLoop_Poll();

    void processEvents() override;

    //operator for IO event
    void addIoEvent(int fd, ioCallback proc, int mask, std::any args) override;
    //delete only mask event for fd in epoll
    void delIoEvent(int fd, int mask) override;
    //delete event for fd in epoll
    void delIoEvent(int fd) override;

    std::unordered_set<int>& getAllListenings() { return m_listening; }

    void addTask(pendingFunc func, std::any args) override;

    void runTask() override;

    int runAt(timerCallback cb, std::any args, time_t ts) override;
    int runAfter(timerCallback cb, std::any args, int sec, int millis = 0) override;
    int runEvery(timerCallback cb, std::any args, int sec, int millis = 0) override;
    void delTimer(int timer_id) override;


    std::shared_ptr<TimerQueue>& getTimerQueue() { return m_timer_queue; }
    

private:
    struct pollfd m_fds[1024];
    int m_nfds{0};

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
