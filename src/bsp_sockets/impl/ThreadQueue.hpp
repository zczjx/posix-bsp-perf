/*
MIT License

Copyright (c) 2024 Clarence Zhou<287334895@qq.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/


#ifndef __THREAD_QUEUE_H__
#define __THREAD_QUEUE_H__

#include <queue>
#include <algorithm>
#include <mutex>
#include <memory>
#include <functional>
#include <any>

#include <sys/eventfd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <shared/BspLogger.hpp>
#include <bsp_sockets/EventLoop.hpp>
#include "BspSocketException.hpp"

namespace bsp_sockets
{
using namespace bsp_perf::shared;

template <typename T>
class ThreadQueue
{
public:
    ThreadQueue():
        m_event_fd{::eventfd(0, EFD_NONBLOCK)},
        m_logger{std::make_unique<BspLogger>("ThreadQueue")}
    {
        if (m_event_fd == -1)
        {
            throw BspSocketException("m_event_fd(0, EFD_NONBLOCK)");
        }
    }

    virtual ~ThreadQueue()
    {
        m_loop->delIoEvent(m_event_fd);

        while (!m_queue.empty())
        {
            m_queue.pop();
        }

        ::close(m_event_fd);
    }

    void sendMsg(const T& item)
    {
        std::lock_guard<std::mutex> lockGuard(m_mutex);

        uint64_t number = 1;
        m_queue.push(item);
        int ret = ::write(m_event_fd, &number, sizeof(uint64_t));
        if (ret == -1)
        {
            m_logger->printStdoutLog(BspLogger::LogLevel::Error, "eventfd write");
        }
    }

    void recvMsg(std::queue<T>& tmp_queue)
    {
        std::lock_guard<std::mutex> lockGuard(m_mutex);

        uint64_t number;
        int ret = ::read(m_event_fd, &number, sizeof(uint64_t));

        if (ret == -1)
        {
            m_logger->printStdoutLog(BspLogger::LogLevel::Error, "eventfd read");
        }
        std::swap(tmp_queue, m_queue);
    }

    std::shared_ptr<EventLoop> getLoop() { return m_loop; }

    //set loop and install message comming event's callback: proc
    void setLoop(std::shared_ptr<EventLoop> loop, ioCallback proc, std::any args)
    {
        m_loop = loop;
        m_loop->addIoEvent(m_event_fd, proc, EPOLLIN, args);
    }

private:
    std::unique_ptr<bsp_perf::shared::BspLogger> m_logger;
    int m_event_fd{-1};
    std::shared_ptr<EventLoop> m_loop{nullptr};
    std::queue<T> m_queue{};
    std::mutex m_mutex{};
};

}

#endif
