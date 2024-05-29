#ifndef __TIMER_QUEUE_H__
#define __TIMER_QUEUE_H__

#include <stdint.h>
#include <vector>
#include <ext/hash_map>

class EventLoop;


typedef void timer_callback(EventLoop& loop, std::any usr_data);//Timer事件回调函数

struct timer_event//注册的Timer事件
{
    timer_event(timer_callback* timo_cb, void* data, uint64_t arg_ts, uint32_t arg_int = 0):
    cb(timo_cb), cb_data(data), ts(arg_ts), interval(arg_int)
    {
    }

    timer_callback* cb;
    void* cb_data;
    uint64_t ts;
    uint32_t interval;//interval millis
    int timer_id;
};

class timer_queue
{
public:
    timer_queue();
    ~timer_queue();

    int add_timer(timer_event& te);

    void del_timer(int timer_id);

    int notifier() const { return _timerfd; }
    int size() const { return _count; }

    void get_timo(std::vector<timer_event>& fired_evs);
private:
    void reset_timo();

    //heap operation
    void heap_add(timer_event& te);
    void heap_del(int pos);
    void heap_pop();
    void heap_hold(int pos);

    std::vector<timer_event> _event_lst;
    typedef std::vector<timer_event>::iterator vit;

    __gnu_cxx::hash_map<int, int> _position;

    typedef __gnu_cxx::hash_map<int, int>::iterator mit;
    int _count;
    int _next_timer_id;
    int _timerfd;
    uint64_t _pioneer;//recent timer's millis
};

#endif
