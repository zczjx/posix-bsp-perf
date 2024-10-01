#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "MsgHead.hpp"
#include <bsp_sockets/TcpClient.hpp>
#include "BspSocketException.hpp"
#include <vector>

namespace bsp_sockets
{

using namespace bsp_perf::shared;

static void readCallback(std::shared_ptr<IEventLoop> loop, int fd, std::any args)
{
    std::shared_ptr<TcpClient> client = std::any_cast<std::shared_ptr<TcpClient>>(args);
    client->handleRead();
}

static void writeCallback(std::shared_ptr<IEventLoop> loop, int fd, std::any args)
{
    std::shared_ptr<TcpClient> client = std::any_cast<std::shared_ptr<TcpClient>>(args);
    client->handleWrite();
}

static void reConnectCallback(std::shared_ptr<IEventLoop> loop, std::any usr_data)
{
    std::shared_ptr<TcpClient> client = std::any_cast<std::shared_ptr<TcpClient>>(usr_data);
    client->doConnect();
}

static void connectEventCallback(std::shared_ptr<IEventLoop> loop, int fd, std::any args)
{
    std::shared_ptr<TcpClient> client = std::any_cast<std::shared_ptr<TcpClient>>(args);
    loop->delIoEvent(fd);
    int result;
    socklen_t result_len = sizeof(result);
    getsockopt(fd, SOL_SOCKET, SO_ERROR, &result, &result_len);

    if (result == 0)
    {
        //connect build success!
        client->setNetConnected(true);
        //call on connection callback(if has)
        client->onConnect();
        loop->addIoEvent(fd, readCallback, EPOLLIN, client);

        if (!client->isOutputBufferEmpty())
        {
            loop->addIoEvent(fd, writeCallback, EPOLLOUT, client);
        }
    }
    else
    {
        //connect build error!
        //reconnection after 2s
        loop->runAfter(reConnectCallback, client, 2);
    }
}

TcpClient::TcpClient(std::shared_ptr<IEventLoop> loop, ArgParser&& args):
    m_loop(loop),
    m_args{std::move(args)},
    m_logger{std::make_unique<BspLogger>("TcpClient")}
{
    m_args.getOptionVal("--server_ip", m_client_params.ip_addr);
    m_args.getOptionVal("--server_port", m_client_params.port);
    m_args.getOptionVal("--name", m_client_params.name);

    m_logger->setPattern();

    //construct server address
    ::bzero(&m_remote_server_addr, sizeof(struct sockaddr_in));
    m_remote_server_addr.sin_family = AF_INET;
    int ret = ::inet_aton(m_client_params.ip_addr.c_str(), &m_remote_server_addr.sin_addr);
    if (ret == 0)
    {
        throw BspSocketException("ip format");
    }
    m_remote_server_addr.sin_port = htons(m_client_params.port);

}

void TcpClient::startLoop()
{
    if (m_running.load())
    {
        return;
    }
    //connect
    doConnect();
    m_running.store(true);
    m_loop->processEvents();
}

void TcpClient::stop()
{
    if (!m_running.load())
    {
        return;
    }

    if (m_sockfd != -1)
    {
        m_loop->delIoEvent(m_sockfd);
        ::close(m_sockfd);
        m_sockfd = -1;
    }
    m_running.store(false);
}

void TcpClient::onConnect()
{
    BSP_TRACE_EVENT_BEGIN("TcpClient::onConnect");
    m_logger->printStdoutLog(BspLogger::LogLevel::Info, "connect {}:{} successfully",
            ::inet_ntoa(m_remote_server_addr.sin_addr), ntohs(m_remote_server_addr.sin_port));

    if (m_on_connection_func)
    {
        m_on_connection_func(shared_from_this(), m_on_connection_args);
    }
    BSP_TRACE_EVENT_END();
}

void TcpClient::onClose()
{
    if (m_on_close_func)
    {
        m_on_close_func(shared_from_this(), m_on_close_args);
    }
}

void TcpClient::doConnect()
{
    BSP_TRACE_EVENT_BEGIN("TcpClient::doConnect");
    if (m_sockfd != -1)
    {
        ::close(m_sockfd);
        m_sockfd = -1;
    }
    //create socket
    m_sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, IPPROTO_TCP);
    if (m_sockfd == -1)
    {
        throw BspSocketException("socket()");
    }

    int ret = ::connect(m_sockfd, reinterpret_cast<const struct sockaddr*>(&m_remote_server_addr), m_addrlen);

    if (ret == 0)
    {
        setNetConnected(true);
        //call on connection callback(if has)
        onConnect();
    }
    else
    {
        if (errno == EINPROGRESS)
        {
            //add connection event
            m_loop->addIoEvent(m_sockfd, connectEventCallback, EPOLLOUT, shared_from_this());
        }
        else
        {
            throw BspSocketException("connect()");
        }
    }
    BSP_TRACE_EVENT_END();
}

int TcpClient::sendData(size_t cmd_id, std::vector<uint8_t>& data) //call by user
{
    BSP_TRACE_EVENT_BEGIN("TcpClient::sendData");
    if (!m_net_connected)
    {
        return -1;
    }

    bool need = (m_outbuf_queue.isEmpty()) ? true: false;//if need to add to event loop

    if (m_outbuf_queue.isFull())
    {
        m_logger->printStdoutLog(BspLogger::LogLevel::Error, "no more space to write socket");
        return -1;
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

    if (need)
    {
        m_loop->addIoEvent(m_sockfd, writeCallback, EPOLLOUT, shared_from_this());
    }

    BSP_TRACE_EVENT_END();
    return 0;

}

int TcpClient::handleRead()
{
    //一次性读出来所有数据
    if (!m_net_connected)
    {
        return -1;
    }

    int rn;

    if (::ioctl(m_sockfd, FIONREAD, &rn) == -1)
    {
        m_logger->printStdoutLog(BspLogger::LogLevel::Error, "ioctl FIONREAD");
        return -1;
    }

    int ret;
    std::vector<uint8_t> buffer(rn);

    do
    {
        ret = ::read(m_sockfd, buffer.data(), rn);
    }
    while (ret == -1 && errno == EINTR);

    if ((ret > 0) && (ret == rn))
    {
        msgHead head;

        std::memcpy(&head, buffer.data(), sizeof(msgHead));

        if (!m_msg_dispatcher.exist(head.cmd_id))
        {
            m_logger->printStdoutLog(BspLogger::LogLevel::Error, "this message has no corresponding callback, close connection");
            cleanConnection();
            return -1;
        }

        std::vector<uint8_t> data_buffer(buffer.begin() + sizeof(msgHead), buffer.end());
        m_msg_dispatcher.callbackFunc(head.cmd_id, data_buffer, shared_from_this());

    }
    else if (ret == 0)
    {
        //peer close connection
        m_logger->printStdoutLog(BspLogger::LogLevel::Info, "{} client: connection closed by peer", m_client_params.name);
        cleanConnection();
        return -1;
    }
    else if (ret == -1)
    {
        assert(errno != EAGAIN);
        m_logger->printStdoutLog(BspLogger::LogLevel::Error, "read()");
        cleanConnection();
        return -1;
    }
    else
    {
        m_logger->printStdoutLog(BspLogger::LogLevel::Error, "read() error");
        return -1;
    }

    return 0;
}

int TcpClient::handleWrite()
{
    if (!m_net_connected)
    {
        return -1;
    }

    if (m_outbuf_queue.isEmpty())
    {
        return 0;
    }

    int ret;

    while (!m_outbuf_queue.isEmpty())
    {
        auto& buffer = m_outbuf_queue.getFrontBuffer();

        do
        {
            ret = ::write(m_sockfd, buffer.data(), buffer.size());
        }
        while (ret == -1 && errno == EINTR);

        if (ret > 0)
        {
            m_outbuf_queue.popBuffer();
        }
        else if (ret == -1 && errno != EAGAIN)
        {
            m_logger->printStdoutLog(BspLogger::LogLevel::Error, "write()");
            cleanConnection();
            return -1;
        }
        else
        {
            //此时不可继续写
            break;
        }
    }

    if (m_outbuf_queue.isEmpty())
    {
        m_loop->delIoEvent(m_sockfd, EPOLLOUT);
    }

    return 0;
}

void TcpClient::cleanConnection()
{
    BSP_TRACE_EVENT_BEGIN("TcpClient::cleanConnection");
    if (m_sockfd != -1)
    {
        m_loop->delIoEvent(m_sockfd);
        ::close(m_sockfd);
        m_sockfd = -1;
    }
    setNetConnected(false);

    //call callback when on connection close
    onClose();

    //connect
    doConnect();
    BSP_TRACE_EVENT_END();
}

} // namespace bsp_sockets

