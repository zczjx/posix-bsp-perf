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
    

private:
    struct pollfd m_fds[1024];
    int m_nfds{0};

    
    friend void timerQueueCallback(EventLoop& loop, int fd, std::any args);

};

}

#endif
