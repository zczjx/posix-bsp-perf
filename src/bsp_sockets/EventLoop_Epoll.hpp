#ifndef __EVENT_LOOP_EPOLL_H__
#define __EVENT_LOOP_EPOLL_H__

#include "EventLoop.hpp"


namespace bsp_sockets
{

class EventLoop_Epoll:public EventLoop, public std::enable_shared_from_this<EventLoop_Epoll>
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


private:
    int m_epoll_fd{-1};
    struct epoll_event m_fired_events[MAX_EVENTS];

    friend void timerQueueCallback(EventLoop& loop, int fd, std::any args);

};

#endif
