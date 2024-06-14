// MIT License
//
// Copyright (c) 2024 Clarence Zhou<287334895@qq.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef __THREAD_POOL_H__
#define __THREAD_POOL_H__

#include "MsgHead.hpp"
#include "ThreadQueue.hpp"
#include <queue>
#include <memory>
#include <any>
#include <vector>

#include <thread>

namespace bsp_sockets
{
using namespace bsp_perf::shared;
class TcpServer;
class ThreadPool
{
public:
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    ThreadPool(int thread_cnt, std::weak_ptr<TcpServer> server);

    virtual ~ThreadPool() = default;


    std::shared_ptr<ThreadQueue<queueMsg>> getNextThread();

    void runTask(int thd_index, pendingFunc task, std::any args);

    void runTask(pendingFunc task, std::any args);


private:
    int m_curr_index{0};
    int m_thread_cnt{0};
    std::vector<std::shared_ptr<ThreadQueue<queueMsg>>> m_pool{};
    std::vector<std::thread> m_threads{};
    std::weak_ptr<TcpServer> m_tcp_server;
};

} // namespace bsp_sockets

#endif
