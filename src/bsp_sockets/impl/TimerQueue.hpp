#ifndef __TIMER_QUEUE_H__
#define __TIMER_QUEUE_H__

#include <stdint.h>
#include <vector>
#include <unordered_map>
#include <functional>
#include <any>
#include <memory>

#include <shared/BspLogger.hpp>

namespace bsp_sockets
{

class EventLoop;

using timerCallback = std::function<void(std::shared_ptr<EventLoop> loop, std::any usr_data)>; //Timer事件回调函数
using timerEvent  = struct timerEvent//注册的Timer事件
{
    timerEvent(timerCallback timer_cb, std::any data, uint64_t arg_ts, uint32_t arg_interval = 0):
    cb(timer_cb), cb_data(data), ts(arg_ts), interval(arg_interval)
    {
    }

    timerCallback cb;
    std::any cb_data;
    uint64_t ts;
    uint32_t interval; //interval millis
    int timer_id;
};

class TimerQueue
{
public:
    TimerQueue();
    ~TimerQueue();

    int addTimer(timerEvent& te);

    void delTimer(int timer_id);

    int getNotifier() const { return m_timer_fd; }

    int size() const { return m_count; }

    void getFiredTimerEvents(std::vector<timerEvent>& fired_evs);

    void resetTimerQueue();

private:
    //heap operation
    void heapAdd(timerEvent& te);
    void heapDel(int pos);
    void heapPop();
    void heapHold(int pos);

private:
    std::vector<timerEvent> m_event_list{};
    using vectorIterator = std::vector<timerEvent>::iterator;

    std::unordered_map<int, int> m_position{};
    using mapIterator = std::unordered_map<int, int>::iterator;
    int m_count{0};
    int m_next_timer_id{0};
    int m_timer_fd{-1};
    uint64_t m_pioneer{0}; //recent timer's millis

    std::unique_ptr<bsp_perf::shared::BspLogger> m_logger;
};

}

#endif
