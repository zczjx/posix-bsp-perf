#include "ThreadPool.hpp"
#include "TcpConnection.hpp"

#include <bsp_sockets/EventLoop.hpp>
#include <bsp_sockets/TcpServer.hpp>

#include <memory>
#include <thread>
#include <queue>
#include <any>
#include <utility>


namespace bsp_sockets
{

static void onMsgComing(std::shared_ptr<EventLoop> loop, int fd, std::any args)
{
    auto params = std::any_cast<std::pair<std::shared_ptr<ThreadQueue<queueMsg>>, std::weak_ptr<TcpServer>>>(args);
    auto queue = params.first;
    auto tcp_server = params.second;
    std::queue<queueMsg> msgs;

    queue->recvMsg(msgs);

    while (!msgs.empty() && !tcp_server.expired())
    {
        auto msg = msgs.front();
        msgs.pop();
        if (msg.cmd_type == queueMsg::MSG_TYPE::NEW_CONN)
        {
            // toDo: new connection
            auto conn = tcp_server.lock()->getConnectionFromPool(msg.connection_fd);

            if (!conn.expired())
            {
                conn.lock()->activate(msg.connection_fd, loop);
            }
            else
            {
                tcp_server.lock()->insertConnectionToPool(std::make_shared<TcpConnection>(msg.connection_fd, loop, tcp_server),
                                                            msg.connection_fd);
            }
        }
        else if (msg.cmd_type == queueMsg::MSG_TYPE::NEW_TASK)
        {
            loop->addTask(msg.task_handle->task, msg.task_handle->args);
        }
        else
        {
            //TODO: other message between threads
            continue;
        }
    }
}

static std::any threadDomain(std::shared_ptr<ThreadQueue<queueMsg>> t_queue, std::weak_ptr<TcpServer> server)
{
    std::shared_ptr<EventLoop> loop = std::make_shared<EventLoop>();

    t_queue->setLoop(loop, onMsgComing, std::make_pair(t_queue, server));
    loop->processEvents();
}

ThreadPool::ThreadPool(int thread_cnt, std::weak_ptr<TcpServer> server):
    m_thread_cnt(thread_cnt),
    m_tcp_server(server)
{
    if ((m_thread_cnt <=0) || (m_thread_cnt > 30))
    {
        throw BspSocketException("error thread_cnt");
    }

    for (int i = 0; i < m_thread_cnt; ++i)
    {
        auto thread_queue = std::make_shared<ThreadQueue<queueMsg>>();
        m_pool.push_back(thread_queue);

        auto thread_func = [](std::shared_ptr<ThreadQueue<queueMsg>> t_queue, std::weak_ptr<TcpServer> server){
            threadDomain(t_queue, server);
        };

        auto thd = std::thread(thread_func, m_pool[i], m_tcp_server);

        if (thd.joinable() == false)
        {
            throw BspSocketException("std::thread create failed");
        }
        m_threads.push_back(std::move(thd));
        m_threads[i].detach();
    }
}

std::shared_ptr<ThreadQueue<queueMsg>> ThreadPool::getNextThread()
{
    if (m_curr_index >= m_thread_cnt)
    {
        m_curr_index = 0;
    }

    return m_pool[m_curr_index++];
}

void ThreadPool::runTask(int thd_index, pendingFunc task, std::any args)
{
    queueMsg msg;
    msg.cmd_type = queueMsg::MSG_TYPE::NEW_TASK;
    msg.task_handle = std::make_shared<msgTask>(task, args);
    auto thread_queue = m_pool[thd_index];
    thread_queue->sendMsg(msg);
}

void ThreadPool::runTask(pendingFunc task, std::any args)
{
    for (int i = 0; i < m_thread_cnt; ++i)
    {
        runTask(i, task, args);
    }
}

}



