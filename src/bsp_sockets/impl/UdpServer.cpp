
#include <bsp_sockets/UdpServer.hpp>
#include "BspSocketException.hpp"
#include "MsgHead.hpp"

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>

namespace bsp_sockets
{

using namespace bsp_perf::shared;

static void readCallback(std::shared_ptr<EventLoop> loop, int fd, std::any args)
{
    std::shared_ptr<UdpServer> server = std::any_cast<std::shared_ptr<UdpServer>>(args);
    server->handleRead();
}

UdpServer::UdpServer(std::shared_ptr<EventLoop> loop, ArgParser&& args):
    m_rbuf(MSG_LENGTH_LIMIT),
    m_wbuf(MSG_LENGTH_LIMIT),
    m_loop(loop),
    m_args{std::move(args)},
    m_logger{std::make_unique<BspLogger>("UdpServer")}
{
    m_args.getOptionVal("--ip", m_server_params.ip_addr);
    m_args.getOptionVal("--port", m_server_params.port);
    m_logger->setPattern();

        //ignore SIGHUP and SIGPIPE
    if (::signal(SIGHUP, SIG_IGN) == SIG_ERR)
    {
        m_logger->printStdoutLog(BspLogger::LogLevel::Error, "signal ignore SIGHUP");
    }

    //create socket
    m_sockfd = ::socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_UDP);

    if (m_sockfd == -1)
    {
        throw BspSocketException("socket()");
    }

    struct sockaddr_in serv_addr;
    ::bzero(&serv_addr, sizeof(struct sockaddr_in));

    serv_addr.sin_family = AF_INET;
    int ret = ::inet_aton(m_server_params.ip_addr.c_str(), &serv_addr.sin_addr);
    if (ret == 0)
    {
        throw BspSocketException("ip format {}", m_server_params.ip_addr.c_str());
    }
    serv_addr.sin_port = htons(m_server_params.port);

    ret = ::bind(m_sockfd, (const struct sockaddr *)&serv_addr, sizeof(struct sockaddr_in));

    if (ret == -1)
    {
        throw BspSocketException("bind()");
    }

    m_logger->printStdoutLog(BspLogger::LogLevel::Info, "UdpServer on {}:{} is running...", m_server_params.ip_addr.c_str(), m_server_params.port);

    m_loop->addIoEvent(m_sockfd, readCallback, EPOLLIN, shared_from_this());



}

UdpServer::~UdpServer()
{
    m_loop->delIoEvent(m_sockfd);
    ::close(m_sockfd);
}

void UdpServer::handleRead()
{
    while (true)
    {
        int pkg_len = ::recvfrom(m_sockfd, m_rbuf.data(), m_rbuf.size(), 0, static_cast<struct sockaddr*> (&m_src_addr), &m_addrlen);
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
            m_logger->printStdoutLog(BspLogger::LogLevel::Error, "data format error in data head, head[%d,%d], pkg_len %d", head.length, head.cmd_id, pkg_len);
            continue;
        }

        m_msg_dispatcher.callbackFunc(m_rbuf.data() + sizeof(msgHead), head.length, head.cmd_id, shared_from_this());
    }

}

int UdpServer::sendData(std::vector<uint8_t>& data, int cmd_id)
{
    if (datlen > MSG_LENGTH_LIMIT)
    {
        m_logger->printStdoutLog(BspLogger::LogLevel::Error, "udp response length too large");
        return -1;
    }

    msgHead head;
    head.length = datlen;
    head.cmd_id = cmd_id;

    std::memcpy(m_wbuf.data(), &head, sizeof(msgHead));
    std::memcpy(m_wbuf.data() + sizeof(msgHead), data.data(), datlen);

    int ret = ::sendto(m_sockfd, m_wbuf.data(), m_wbuf.size(), 0, static_cast<struct sockaddr*> (&m_src_addr), &m_addrlen);

    if (ret == -1)
    {
        m_logger->printStdoutLog(BspLogger::LogLevel::Error, "sendto()");
    }
    return ret;
}

} //namespace bsp_sockets



