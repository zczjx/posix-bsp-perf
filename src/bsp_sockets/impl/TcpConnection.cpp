
#include "TcpConnection.hpp"
#include <bsp_sockets/TcpServer.hpp>
#include <bsp_sockets/MsgHead.hpp>
#include <bsp_sockets/BspSocketException.hpp>

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <netinet/tcp.h>

namespace bsp_sockets
{
using namespace bsp_perf::shared;

TcpConnection::TcpConnection(int conn_fd, std::shared_ptr<EventLoop> loop, std::shared_ptr<TcpServer> server):
    m_connection_fd(conn_fd),
    m_loop(loop),
    m_tcp_server(server),
    m_logger{std::make_unique<BspLogger>()}
{
    activate(conn_fd, loop);
}

void TcpConnection::activate(int conn_fd, std::shared_ptr<EventLoop> loop)
{
    m_connection_fd = conn_fd;
    m_loop = loop;
    //set NONBLOCK
    int flag = ::fcntl(m_connection_fd, F_GETFL, 0);
    ::fcntl(m_connection_fd, F_SETFL, O_NONBLOCK | flag);

    //set NODELAY
    int opend = 1;
    int ret = ::setsockopt(m_connection_fd, IPPROTO_TCP, TCP_NODELAY, &opend, sizeof(opend));
    if (ret < 0)
    {
        throw BspSocketException("setsockopt TCP_NODELAY");
    }

    if (!m_tcp_server.expired())
    {
        std::shared_ptr<TcpServer> server = m_tcp_server.lock();
        server->connectionEstablishCb(this);
        server->incConnection();
    }

    auto tcp_rcb = [](std::shared_ptr<EventLoop> loop, int fd, std::any args)
    {
        std::shared_ptr<TcpConnection> conn = std::any_cast<std::shared_ptr<TcpConnection>>(args);
        conn->handleRead();
    };

    m_loop->addIoEvent(m_connection_fd, tcp_rcb, EPOLLIN, this);
}


void TcpConnection::handleRead()
{
    int ret = m_in_buf.readData(m_connection_fd);
    if (ret == -1)
    {
        //read data error
        m_logger->printStdoutLog(BspLogger::LogLevel::Error, "read data from socket");
        cleanConnection();
        return ;
    }
    else if (ret == 0)
    {
        //The peer is closed, return -2
        m_logger->printStdoutLog(BspLogger::LogLevel::Debug, "connection closed by peer");
        cleanConnection();
        return ;
    }
    commu_head head;
    while (m_in_buf.length() >= COMMU_HEAD_LENGTH)
    {
        ::memcpy(&head, m_in_buf.data(), COMMU_HEAD_LENGTH);
        if (head.length > MSG_LENGTH_LIMIT || head.length < 0)
        {
            //data format is messed up
            m_logger->printStdoutLog(BspLogger::LogLevel::Error, "data format error in data head, close connection");
            cleanConnection();
            break;
        }

        if (m_in_buf.length() < COMMU_HEAD_LENGTH + head.length)
        {
            //this is half-package
            break;
        }

        if (!m_tcp_server.expired())
        {
            std::shared_ptr<TcpServer> server = m_tcp_server.lock();
            auto& dispatcher = server->getMsgDispatcher();
            if (!dispatcher.exist(head.cmdid))
            {
                //data format is messed up
                m_logger->printStdoutLog(BspLogger::LogLevel::Error, "this message has no corresponding callback, close connection");
                cleanConnection();
                break;
            }
            m_in_buf.pop(COMMU_HEAD_LENGTH);
            //domain: call user callback
            dispatcher.cb(m_in_buf.data(), head.length, head.cmdid, this);
            m_in_buf.pop(head.length);
        }
        else
        {
            m_logger->printStdoutLog(BspLogger::LogLevel::Error, "TcpServer is expired");
            cleanConnection();
            break;
        }
    }

    m_in_buf.adjust();
}

void TcpConnection::handleWrite()
{
    while (m_out_buf.length())
    {
        int ret = m_out_buf.writeData(m_connection_fd);
        if (ret == -1)
        {
            m_logger->printStdoutLog(BspLogger::LogLevel::Error, "write data to socket");
            cleanConnection();
            return ;
        }
        if (ret == 0)
        {
            //不是错误，仅返回为0表示此时不可继续写
            break;
        }
    }

    if (!m_out_buf.length())
    {
        m_loop->delIoEvent(m_connection_fd, EPOLLOUT);
    }

}

int TcpConnection::sendData(std::span<const uint8_t> data, int datlen, int cmdid)
{
    bool need_listen = false;

    if (!m_out_buf.length())
    {
        need_listen = true;
    }

    //write rsp head first
    commu_head head;
    head.cmdid = cmdid;
    head.length = datlen;
    //write head
    auto ret = m_out_buf.sendData(reinterpret_cast<const uint8_t*>(&head), COMMU_HEAD_LENGTH);

    if (ret != 0)
    {
        return -1;
    }

    //write content
    ret = m_out_buf.sendData(data.data(), datlen);

    if (ret != 0)
    {
        //只好取消写入的消息头
        m_out_buf.pop(COMMU_HEAD_LENGTH);
        return -1;
    }

    auto tcp_wcb = [](std::shared_ptr<EventLoop> loop, int fd, std::any args)
    {
        std::shared_ptr<TcpConnection> conn = std::any_cast<std::shared_ptr<TcpConnection>>(args);
        conn->handleWrite();
    };

    if (need_listen)
    {
        m_loop->addIoEvent(m_connection_fd, tcp_wcb, EPOLLOUT, this);
    }

    return 0;
}

void TcpConnection::cleanConnection()
{
    if (!m_tcp_server.expired())
    {
        std::shared_ptr<TcpServer> server = m_tcp_server.lock();
        server->connectionCloseCb(this);
        server->decConnection();
    }

    m_loop->delIoEvent(m_connection_fd);
    m_in_buf.clear();
    m_out_buf.clear();
    ::close(m_connection_fd);
    m_connection_fd = -1;
}

} // namespace bsp_sockets