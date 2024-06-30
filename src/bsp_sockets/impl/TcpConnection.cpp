
#include "TcpConnection.hpp"
#include "MsgHead.hpp"
#include <bsp_sockets/TcpServer.hpp>
#include "BspSocketException.hpp"

#include <cstring>
#include <memory>
#include <vector>

#include <fcntl.h>
#include <unistd.h>
#include <netinet/tcp.h>

namespace bsp_sockets
{
using namespace bsp_perf::shared;

TcpConnection::TcpConnection(int conn_fd, std::shared_ptr<EventLoop> loop, std::weak_ptr<TcpServer> server):
    m_connection_fd(conn_fd),
    m_loop(loop),
    m_tcp_server(server),
    m_logger{std::make_unique<BspLogger>("TcpConnection")}
{
    m_logger->setPattern();
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
        if (server->onConnectionEstablish != nullptr)
        {
            server->onConnectionEstablish(shared_from_this());
        }
        server->incConnection();
    }

    auto tcp_rcb = [](std::shared_ptr<EventLoop> loop, int fd, std::any args)
    {
        std::shared_ptr<TcpConnection> conn = std::any_cast<std::shared_ptr<TcpConnection>>(args);
        conn->handleRead();
    };

    m_loop->addIoEvent(m_connection_fd, tcp_rcb, EPOLLIN, shared_from_this());
}


void TcpConnection::handleRead()
{
    int ret = m_inbuf_queue.readData(m_connection_fd);

    if (ret < 0)
    {
        //read data error
        m_logger->printStdoutLog(BspLogger::LogLevel::Error, "read data from socket");
        cleanConnection();
        return;
    }
    else if (ret == 0)
    {
        //The peer is closed, return -2
        m_logger->printStdoutLog(BspLogger::LogLevel::Debug, "connection closed by peer ret: {0:d}", ret);
        cleanConnection();
        return;
    }

    msgHead head;

    while (m_inbuf_queue.getBuffersCount() > 0)
    {
        auto& in_buffer = m_inbuf_queue.getFrontBuffer();
        std::memcpy(&head, in_buffer.data(), sizeof(msgHead));

        if (head.length > MSG_LENGTH_LIMIT || head.length < 0)
        {
            //data format is messed up
            m_logger->printStdoutLog(BspLogger::LogLevel::Error, "data format error in data head, close connection");
            cleanConnection();
            break;
        }

        if (in_buffer.size() < MSG_HEAD_LENGTH + head.length)
        {
            //this is half-package
            break;
        }

        if (!m_tcp_server.expired())
        {
            std::shared_ptr<TcpServer> server = m_tcp_server.lock();
            auto& dispatcher = server->getMsgDispatcher();

            m_logger->printStdoutLog(BspLogger::LogLevel::Error, "head.cmd_id: 0x{0:x}", head.cmd_id);
            if (!dispatcher.exist(head.cmd_id))
            {
                //data format is messed up
                m_logger->printStdoutLog(BspLogger::LogLevel::Error, "this message has no corresponding callback, close connection");
                cleanConnection();
                break;
            }

            std::vector<uint8_t> data_buffer(in_buffer.begin() + sizeof(msgHead), in_buffer.end());
            //domain: call user callback
            dispatcher.callbackFunc(head.cmd_id, data_buffer, shared_from_this());
            m_inbuf_queue.popBuffer();
        }
        else
        {
            m_logger->printStdoutLog(BspLogger::LogLevel::Error, "TcpServer is expired");
            cleanConnection();
            break;
        }
    }
}

void TcpConnection::handleWrite()
{
    while (m_outbuf_queue.getBuffersCount() > 0)
    {
        int ret = m_outbuf_queue.writeFd(m_connection_fd);
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

    if (m_outbuf_queue.isEmpty())
    {
        m_loop->delIoEvent(m_connection_fd, EPOLLOUT);
    }

}

int TcpConnection::sendData(size_t cmd_id, std::vector<uint8_t>& data)
{
    bool need_listen = false;

    if (m_outbuf_queue.isEmpty())
    {
        need_listen = true;
    }

    std::vector<uint8_t> buffer(sizeof(msgHead));
    //write rsp head first
    msgHead head;
    head.cmd_id = cmd_id;
    head.length = data.size();
    std::memcpy(buffer.data(), &head, sizeof(msgHead));
    buffer.insert(buffer.end(), data.begin(), data.end());

    auto ret = m_outbuf_queue.sendData(buffer);

    if (ret != 0)
    {
        return -1;
    }

    auto tcp_wcb = [](std::shared_ptr<EventLoop> loop, int fd, std::any args)
    {
        std::shared_ptr<TcpConnection> conn = std::any_cast<std::shared_ptr<TcpConnection>>(args);
        conn->handleWrite();
    };

    if (need_listen)
    {
        m_loop->addIoEvent(m_connection_fd, tcp_wcb, EPOLLOUT, shared_from_this());
    }

    return 0;
}

void TcpConnection::cleanConnection()
{
    if (!m_tcp_server.expired())
    {
        std::shared_ptr<TcpServer> server = m_tcp_server.lock();
        if (server->onConnectionClose != nullptr)
        {
            server->onConnectionClose(shared_from_this());
        }
        server->decConnection();
    }

    m_loop->delIoEvent(m_connection_fd);
    m_inbuf_queue.clearBuffers();
    m_outbuf_queue.clearBuffers();
    ::close(m_connection_fd);
    m_connection_fd = -1;
}

} // namespace bsp_sockets