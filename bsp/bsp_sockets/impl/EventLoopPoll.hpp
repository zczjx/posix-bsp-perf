#ifndef __EVENT_LOOP_POLL_H__
#define __EVENT_LOOP_POLL_H__

#include <bsp_sockets/IEventLoop.hpp>

//#define USE_EPOLL

namespace bsp_sockets
{

class EventLoopPoll : public IEventLoop, public std::enable_shared_from_this<EventLoopPoll>
{
public:
    EventLoopPoll();

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
    EventLoopParams m_params{};

    friend void timerQueueCallback(IEventLoop& loop, int fd, std::any args);

};

}
#endif
