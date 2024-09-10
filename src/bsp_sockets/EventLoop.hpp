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
class EventLoop
{
public:
    EventLoop(){}

    virtual void processEvents() {}

    //operator for IO event
    virtual void addIoEvent(int fd, ioCallback proc, int mask, std::any args) {}
    //delete only mask event for fd in epoll
    virtual void delIoEvent(int fd, int mask) {}
    //delete event for fd in epoll
    virtual void delIoEvent(int fd) {}
    //get all fds this loop is listening
    virtual std::unordered_set<int>& getAllListenings() {}

    virtual void addTask(pendingFunc func, std::any args) {}
    virtual void runTask() {}

    virtual std::shared_ptr<TimerQueue>& getTimerQueue() {}
    //operator for timer event
    virtual int runAt(timerCallback cb, std::any args, time_t ts) {}
    virtual int runAfter(timerCallback cb, std::any args, int sec, int millis = 0){}
    virtual int runEvery(timerCallback cb, std::any args, int sec, int millis = 0){}
    virtual void delTimer(int timer_id){}

    static std::shared_ptr<EventLoop> create(const int32_t flag);

    virtual ~EventLoop() = default;
};

}
#endif
