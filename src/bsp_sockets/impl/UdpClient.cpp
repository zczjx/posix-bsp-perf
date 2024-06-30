#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <bsp_sockets/UdpClient.hpp>
#include "BspSocketException.hpp"
#include "MsgHead.hpp"


namespace bsp_sockets
{
using namespace bsp_perf::shared;

static void readCallback(std::shared_ptr<EventLoop> loop, int fd, std::any args)
{
    std::shared_ptr<UdpClient> client = std::any_cast<std::shared_ptr<UdpClient>>(args);
    client->handleRead();
}


UdpClient::UdpClient(std::shared_ptr<EventLoop> loop, ArgParser&& args):
    m_rbuf(MSG_LENGTH_LIMIT),
    m_wbuf(MSG_LENGTH_LIMIT),
    m_loop(loop),
    m_args{std::move(args)},
    m_logger{std::make_unique<BspLogger>("UdpClient")}

{
    m_args.getOptionVal("--server_ip", m_client_params.ip_addr);
    m_args.getOptionVal("--server_port", m_client_params.port);
    m_logger->setPattern();

    //create socket
    m_sockfd = ::socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_UDP);

    if (m_sockfd == -1)
    {
        throw BspSocketException("socket()");
    }

    struct sockaddr_in serv_addr;
    ::bzero(&serv_addr, sizeof(struct sockaddr_in));

    serv_addr.sin_family = AF_INET;
    int ret = ::inet_aton(m_client_params.ip_addr.c_str(), &serv_addr.sin_addr);
    if (ret == 0)
    {
        throw BspSocketException(std::string("ip format ") + m_client_params.ip_addr);
    }
    serv_addr.sin_port = htons(m_client_params.port);

    ret = ::connect(m_sockfd, (const struct sockaddr*)&serv_addr, sizeof(serv_addr));

    if (ret == -1)
    {
        throw BspSocketException("connect()");
    }

    m_logger->printStdoutLog(BspLogger::LogLevel::Info, "successfully connected on {}:{} ...", m_client_params.ip_addr, m_client_params.port);
}

UdpClient::~UdpClient()
{
    stop();
}

void UdpClient::startLoop()
{
    if (m_running.load())
    {
        return;
    }

    //add accepter event
    m_loop->addIoEvent(m_sockfd, readCallback, EPOLLIN, shared_from_this());

    if (onConnectFunc)
    {
        onConnectFunc(shared_from_this(), m_on_connect_args);
    }
    m_running.store(true);
    m_loop->processEvents();
}

void UdpClient::stop()
{
    if (!m_running.load())
    {
        return;
    }

    m_loop->delIoEvent(m_sockfd);
    ::close(m_sockfd);
    m_running.store(false);
}

void UdpClient::handleRead()
{
    while (true)
    {
        int pkg_len = ::recvfrom(m_sockfd, m_rbuf.data(), m_rbuf.size(), 0, NULL, NULL);

        if (pkg_len == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }
            else if (errno == EAGAIN)
            {
                break;
            }
            else
            {
                m_logger->printStdoutLog(BspLogger::LogLevel::Error, "recfrom()");
                break;
            }
        }
        //handle package _rbuf[0:pkg_len)
        msgHead head;
        std::memcpy(&head, m_rbuf.data(), sizeof(msgHead));
        if ((head.length > MSG_LENGTH_LIMIT) || (head.length < 0) || (head.length + sizeof(msgHead) != pkg_len))
        {
            //data format is messed up
            m_logger->printStdoutLog(BspLogger::LogLevel::Error, "data format error in data head");
            continue;
        }
        std::vector<uint8_t> data_buffer(m_rbuf.begin() + sizeof(msgHead), m_rbuf.end());
        m_msg_dispatcher.callbackFunc(head.cmd_id, data_buffer, shared_from_this());
    }

}


int UdpClient::sendData(size_t cmd_id, std::vector<uint8_t>& data)
{
    if (data.size() > MSG_LENGTH_LIMIT)
    {
        m_logger->printStdoutLog(BspLogger::LogLevel::Error, "udp response length too large");
        return -1;
    }

    msgHead head;
    head.length = data.size();
    head.cmd_id = cmd_id;

    std::memcpy(m_wbuf.data(), &head, sizeof(msgHead));
    m_wbuf.insert(m_wbuf.begin() + sizeof(msgHead), data.begin(), data.end());
    auto data_len = sizeof(msgHead) + head.length;
    auto ret = ::send(m_sockfd, m_wbuf.data(), data_len, 0);

    if (ret < 0)
    {
        m_logger->printStdoutLog(BspLogger::LogLevel::Error, "send() ret: {}", ret);
    }

    return ret;

}

}