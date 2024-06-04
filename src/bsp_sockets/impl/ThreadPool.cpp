#include "ThreadPool.hpp"
#include "TcpConnection.hpp"
#include "BspSocketException.hpp"

#include <bsp_sockets/EventLoop.hpp>
#include <bsp_sockets/TcpServer.hpp>


namespace bsp_sockets
{
using namespace bsp_perf::shared;

static void onMsgComing(std::shared_ptr<EventLoop> loop, int fd, std::any args)
{
    auto queue = std::any_cast<std::shared_ptr<ThreadQueue<queueMsg>>>(args);
    std::queue<queueMsg> msgs;
    queue->recvMsg(msgs);

    while (!msgs.empty())
    {
        auto msg = msgs.front();
        msgs.pop();
        if (msg.cmd_type == queueMsg::MSG_TYPE::NEW_CONN)
        {
            // toDo: new connection
            tcp_conn* conn = tcp_server::conns[msg.connfd];
            if (conn)
            {
                conn->init(msg.connfd, loop);
            }
            else
            {
                tcp_server::conns[msg.connfd] = new tcp_conn(msg.connfd, loop);
                exit_if(tcp_server::conns[msg.connfd] == NULL, "new tcp_conn");
            }
        }
        else if (msg.cmd_type == queue_msg::NEW_TASK)
        {
            loop->add_task(msg.task, msg.args);
        }
        else
        {
            //TODO: other message between threads
        }
    }
}

static std::any threadDomain(std::any args)
{
    auto queue = std::any_cast<std::shared_ptr<ThreadQueue<queueMsg>>>(args);
    std::shared_ptr<EventLoop> loop = std::make_shared<EventLoop>();

    loop->setLoop(func, queue);
    loop->processEvents();

    auto func = [](std::shared_ptr<EventLoop> loop, int fd, std::any args)
    {
        onMsgComing(loop, fd, args);
    };

    queue->setLoop(loop, func, queue);
    loop->processEvents();
}

ThreadPool::ThreadPool(int thread_cnt):
    m_thread_cnt(thread_cnt)
{
    if ((m_thread_cnt <=0) || (m_thread_cnt > 30))
    {
        throw BspSocketException("error thread_cnt");
    }

    for (int i = 0; i < m_thread_cnt; ++i)
    {
        m_pool.push_back(std::make_shared<ThreadQueue<queue_msg>>());

        auto func = [](std::any args)
        {
            threadDomain(args);
        };

        auto thd = std::thread(func, m_pool[i]);

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



