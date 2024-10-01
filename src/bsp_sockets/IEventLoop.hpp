#ifndef __IEVENT_LOOP_H__
#define __IEVENT_LOOP_H__

#include "impl/TimerQueue.hpp"
#include <shared/BspLogger.hpp>
#include <sys/epoll.h>
#include <unordered_map>
#include <unordered_set>
#include <any>
#include <functional>
#include <iostream>
#include <memory>
#include <poll.h>

namespace bsp_sockets
{
static constexpr int MAX_EVENTS = 10;

using ioCallback = std::function<void(std::shared_ptr<IEventLoop> loop, int fd, std::any args)>;
//让当前loop在一次poll循环后执行指定任务
using pendingFunc = std::function<void(std::shared_ptr<IEventLoop>, std::any)>;

struct IOEvent//注册的IO事件
{
    int mask{0x00};             //EPOLLIN EPOLLOUT
    ioCallback read_callback{nullptr};  //callback when EPOLLIN comming
    ioCallback write_callback{nullptr}; //callback when EPOLLOUT comming
    std::any rcb_args{nullptr};   //extra arguments for read_cb
    std::any wcb_args{nullptr};  //extra arguments for write_cb
};

struct EventLoopParams
{
    int m_epoll_fd{-1};
    struct epoll_event m_fired_events[MAX_EVENTS];

    struct pollfd m_fds[1024];
    int m_nfds{0};
    //map: fd->IOEvent
    std::unordered_map<int, IOEvent> m_io_events{};
    using ioevIterator = std::unordered_map<int, IOEvent>::iterator;
    std::shared_ptr<TimerQueue> m_timer_queue{nullptr};
    //此队列用于:暂存将要执行的任务
    std::vector<std::pair<pendingFunc, std::any> > m_pending_factors{};

    std::unordered_set<int> m_listening{};

    std::unique_ptr<bsp_perf::shared::BspLogger> m_logger;
};

class IEventLoop
{
public:

    virtual void processEvents() = 0;

    //operator for IO event
    virtual void addIoEvent(int fd, ioCallback proc, int mask, std::any args) = 0;
    //delete only mask event for fd in epoll
    virtual void delIoEvent(int fd, int mask) = 0;
    //delete event for fd in epoll
    virtual void delIoEvent(int fd) = 0;
    //get all fds this loop is listening
    virtual std::unordered_set<int>& getAllListenings() = 0;

    virtual void addTask(pendingFunc func, std::any args) = 0;
    virtual void runTask() = 0;

    virtual std::shared_ptr<TimerQueue>& getTimerQueue() = 0;
    //operator for timer event
    virtual int runAt(timerCallback cb, std::any args, time_t ts) =0;
    virtual int runAfter(timerCallback cb, std::any args, int sec, int millis = 0) = 0;
    virtual int runEvery(timerCallback cb, std::any args, int sec, int millis = 0) = 0;
    virtual void delTimer(int timer_id)  = 0;


    static std::shared_ptr<IEventLoop> create(const std::string flag);


};

}
#endif
