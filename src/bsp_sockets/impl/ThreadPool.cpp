#include "ThreadPool.hpp"
#include "TcpConnection.hpp"

#include <bsp_sockets/EventLoop.hpp>
#include <bsp_sockets/TcpServer.hpp>


namespace bsp_sockets
{
void ThreadPool::onMsgComing(std::shared_ptr<EventLoop> loop, int fd, std::any args)
{
    auto queue = std::any_cast<std::shared_ptr<ThreadQueue<queueMsg>>>(args);
    std::queue<queueMsg> msgs;
    queue->recvMsg(msgs);

    while (!msgs.empty() && !m_tcp_server.expired())
    {
        auto msg = msgs.front();
        msgs.pop();
        if (msg.cmd_type == queueMsg::MSG_TYPE::NEW_CONN)
        {
            // toDo: new connection
            auto conn = m_tcp_server.lock()->getConnectionFromPool(msg.connection_fd);

            if (!conn.expired())
            {
                conn->activate(msg.connection_fd, loop);
            }
            else
            {
                m_tcp_server.lock()->insertConnectionToPool(std::make_shared<TcpConnection>(msg.connection_fd, loop, m_tcp_server),
                                                            msg.connection_fd);
            }
        }
        else if (msg.cmd_type == queueMsg::MSG_TYPE::NEW_TASK)
        {
            loop->addTask(msg.task, msg.args);
        }
        else
        {
            //TODO: other message between threads
            continue;
        }
    }
}

std::any ThreadPool::threadDomain(std::any args)
{
    auto queue = std::any_cast<std::shared_ptr<ThreadQueue<queueMsg>>>(args);
    std::shared_ptr<EventLoop> loop = std::make_shared<EventLoop>();

    queue->setLoop(loop, onMsgComing, queue);
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
        m_pool.push_back(std::make_shared<ThreadQueue<queueMsg>>());

        auto thd = std::thread(threadDomain, m_pool[i]);

        if (thd.joinable() == false)
        {
            throw BspSocketException("std::thread create failed");
        }
        m_tids.push_back(std::move(thd));
        m_tids[i].detach();
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
    msg.task = task;
    msg.args = args;
    auto thread_queue = m_pool[thd_index];
    thread_queue->sendMsg(msg);
}

void ThreadPool::runTask(pendingFunc task, std::any args)
{
    for (int i = 0; i < m_thread_cnt; ++i)
    {
        runTask(i, task);
    }
}

}



