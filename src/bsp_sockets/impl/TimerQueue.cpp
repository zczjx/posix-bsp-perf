#include <unistd.h>
#include <strings.h>
#include <pthread.h>
#include <sys/timerfd.h>
#include <algorithm>

#include "TimerQueue.hpp"
#include "BspSocketException.hpp"

namespace bsp_sockets
{
using namespace bsp_perf::shared;

TimerQueue::TimerQueue():
    m_timer_fd{::timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK | TFD_CLOEXEC)}
{
    if (m_timer_fd == -1)
    {
        throw BspSocketException("timerfd_create");
    }
}

TimerQueue::~TimerQueue()
{
    ::close(m_timer_fd);
}

int TimerQueue::addTimer(timerEvent& te)
{
    te.timer_id = m_next_timer_id++;
    heapAdd(te);

    if (m_event_list[0].ts < m_pioneer)
    {
        m_pioneer = m_event_list[0].ts;
        resetTimerQueue();
    }

    return te.timer_id;

}

void TimerQueue::delTimer(int timer_id)
{
    mapIterator it = m_position.find(timer_id);
    if (it == m_position.end())
    {
        m_logger->printStdoutLog(BspLogger::LogLevel::Error, "no such a timerid {}", timer_id);
        return;
    }
    int pos = it->second;
    heapDel(pos);

    if (m_count == 0)
    {
        m_pioneer = -1;
        resetTimerQueue();
    }
    else if (m_event_list[0].ts < m_pioneer)
    {
        m_pioneer = m_event_list[0].ts;
        resetTimerQueue();
    }

}

void TimerQueue::getFiredTimerEvents(std::vector<timerEvent>& fired_evs)
{
    std::vector<timerEvent> reuse_lst;
    while (m_count != 0 && m_pioneer == m_event_list[0].ts)
    {
        timerEvent te = m_event_list[0];
        fired_evs.push_back(te);
        if (te.interval)
        {
            te.ts += te.interval;
            reuse_lst.push_back(te);
        }
        heapPop();
    }
    for (vectorIterator it = reuse_lst.begin(); it != reuse_lst.end(); ++it)
    {
        addTimer(*it);
    }
    //reset timeout
    if (m_count == 0)
    {
        m_pioneer = -1;
        resetTimerQueue();
    }
    else//m_pioneer != m_event_list[0].ts
    {
        m_pioneer = m_event_list[0].ts;
        resetTimerQueue();
    }
}

void TimerQueue::resetTimerQueue()
{
    struct itimerspec old_ts, new_ts;
    ::bzero(&new_ts, sizeof(new_ts));

    if (m_pioneer != (uint64_t)-1)
    {
        new_ts.it_value.tv_sec = m_pioneer / 1000;
        new_ts.it_value.tv_nsec = (m_pioneer % 1000) * 1000000;
    }
    //when m_pioneer = -1, new_ts = 0 will disarms the timer
    ::timerfd_settime(m_timer_fd, TFD_TIMER_ABSTIME, &new_ts, &old_ts);
}

void TimerQueue::heapAdd(timerEvent& te)
{
    m_event_list.push_back(te);
    //update position
    m_position[te.timer_id] = m_count;

    int curr_pos = m_count;
    m_count++;
    int previous_pos = (curr_pos - 1) / 2;

    while (previous_pos >= 0 && m_event_list[curr_pos].ts < m_event_list[previous_pos].ts)
    {
        std::swap(m_event_list[curr_pos], m_event_list[previous_pos]);
        //update position
        m_position[m_event_list[curr_pos].timer_id] = curr_pos;
        m_position[m_event_list[previous_pos].timer_id] = previous_pos;

        curr_pos = previous_pos;
        previous_pos = (curr_pos - 1) / 2;
    }
}

void TimerQueue::heapDel(int pos)
{
    timerEvent to_del = m_event_list[pos];
    //rear item
    timerEvent tmp = m_event_list[m_count - 1];
    m_event_list[pos] = tmp;
    //update position
    m_position[tmp.timer_id] = pos;
    //update position
    m_position.erase(to_del.timer_id);

    m_count--;
    m_event_list.pop_back();


    heapHold(pos);//sink
    if (m_event_list[pos].timer_id == tmp.timer_id)
    {
        heapFloat(pos);//float if pos's timer id is smaller than parent
    }
         
}

void TimerQueue::heapPop()
{
    if (m_count <= 0)
        return ;
    //update position
    m_position.erase(m_event_list[0].timer_id);

    if (m_count > 1)
    {
        timerEvent tmp = m_event_list[m_count - 1];
        m_event_list[0] = tmp;
        //update position
        m_position[tmp.timer_id] = 0;

        m_event_list.pop_back();
        m_count--;
        heapHold(0);
    }
    else if (m_count == 1)
    {
        m_event_list.clear();
        m_count = 0;
    }

}

void TimerQueue::heapHold(int pos)
{
    int left = 2 * pos + 1, right = 2 * pos + 2;
    int min_pos = pos;

    if (left < m_count && m_event_list[min_pos].ts > m_event_list[left].ts)
    {
        min_pos = left;
    }

    if (right < m_count && m_event_list[min_pos].ts > m_event_list[right].ts)
    {
        min_pos = right;
    }

    if (min_pos != pos)
    {
        std::swap(m_event_list[min_pos], m_event_list[pos]);
        //update position
        m_position[m_event_list[min_pos].timer_id] = min_pos;
        m_position[m_event_list[pos].timer_id] = pos;

        heapHold(min_pos);
    }
}

void TimerQueue::heapFloat(int pos)
{
    while (pos > 0) {
        int parent = (pos - 1) / 2;
        if (m_event_list[pos].ts < m_event_list[parent].ts) {
            std::swap(m_event_list[pos], m_event_list[parent]);
            m_position[m_event_list[pos].timer_id] = pos;
            m_position[m_event_list[parent].timer_id] = parent;
            pos = parent;
        } else {
            break;
        }
    }
}

} // namespace bsp_sockets
