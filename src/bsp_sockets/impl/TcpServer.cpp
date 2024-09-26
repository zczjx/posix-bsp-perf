
#include "MsgHead.hpp"
#include <bsp_sockets/TcpServer.hpp>
#include "TcpConnection.hpp"
#include "BspSocketException.hpp"

#include <profiler/BspTrace.hpp>

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <strings.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#include <memory>

namespace bsp_sockets
{
using namespace bsp_perf::shared;

static void connectOnEstablish(std::shared_ptr<ISocketHelper> conn)
{
   std::cout << "TcpServer onConnectionEstablish fd: " << conn->getFd() << std::endl;
}

static void connectOnClose(std::shared_ptr<ISocketHelper> conn)
{
    std::cout << "TcpServer onConnectionClose fd: " << conn->getFd() << std::endl;
}

TcpServer::TcpServer(std::shared_ptr<EventLoop> loop, bsp_perf::shared::ArgParser&& args):
    m_loop(loop),
    m_args{std::move(args)},
    m_logger{std::make_unique<BspLogger>("TcpServer")}
{
    m_args.getOptionVal("--ip", m_server_params.ipaddr);
    m_args.getOptionVal("--port", m_server_params.port);
    m_args.getOptionVal("--thread_num", m_server_params.thread_num);
    m_args.getOptionVal("--max_connections", m_server_params.max_connections);
    m_args.getOptionVal("--poll", m_server_params.pollType);

    m_logger->setPattern();
    ::bzero(&m_conn_addr, sizeof(m_conn_addr));
    //ignore SIGHUP and SIGPIPE
    if (::signal(SIGHUP, SIG_IGN) == SIG_ERR)
    {
        m_logger->printStdoutLog(BspLogger::LogLevel::Error, "signal ignore SIGHUP");
    }
    if (::signal(SIGPIPE, SIG_IGN) == SIG_ERR)
    {
        m_logger->printStdoutLog(BspLogger::LogLevel::Error, "signal ignore SIGPIPE");
    }

    //create socket
    BSP_TRACE_EVENT_BEGIN("tcpserver open socket");
    m_sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);

    if (m_sockfd < 0)
    {
        throw BspSocketException("socket()");
    }
    BSP_TRACE_EVENT_END();

    m_reservfd = ::open("/tmp/reactor_accepter", O_CREAT | O_RDONLY | O_CLOEXEC, 0666);

    if (m_reservfd < 0)
    {
        throw BspSocketException("open()");
    }

    struct sockaddr_in servaddr;
    ::bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;

    int ret = ::inet_aton(m_server_params.ipaddr.c_str(), &servaddr.sin_addr);

    if (ret == 0)
    {
        throw BspSocketException("ip format");
    }
    servaddr.sin_port = ::htons(m_server_params.port);

    int opend = 1;
    ret = ::setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &opend, sizeof(opend));

    if (ret < 0)
    {
        throw BspSocketException("setsockopt SO_REUSEADDR");
    }

    ret = ::bind(m_sockfd, (const struct sockaddr*)&servaddr, sizeof(servaddr));
    if (ret < 0)
    {
        throw BspSocketException("bind()");
    }

    ret = ::listen(m_sockfd, 500);

    if (ret < 0)
    {
        throw BspSocketException("listen()");
    }

    m_logger->printStdoutLog(BspLogger::LogLevel::Debug, "server on {}:{} is running...", m_server_params.ipaddr.c_str(), m_server_params.port);

    int next_fd = ::dup(1);
    m_conns_size = m_server_params.max_connections + next_fd;
    ::close(next_fd);
    m_connections_pool.resize(m_conns_size);
    for (auto& conn : m_connections_pool)
    {
        conn.reset(); // 或者 conn = nullptr;
    }

    setConnectionEstablishCallback(connectOnEstablish);
    setConnectionCloseCallback(connectOnClose);
}

//TcpServer类使用时往往具有程序的完全生命周期，其实并不需要析构函数
TcpServer::~TcpServer()
{
    stop();
    // Clean up the connections
    for (auto& conn : m_connections_pool)
    {
        conn.reset();
    }
}

void TcpServer::startLoop()
{
    BSP_TRACE_EVENT_BEGIN("tcpserver start loop");
    if (m_running.load())
    {
        return;
    }

    if (m_server_params.thread_num > 0)
    {
        BSP_TRACE_EVENT_BEGIN("tcpserver create thread_pool");
        m_thread_pool = std::make_shared<ThreadPool>(m_server_params.thread_num, shared_from_this());
        if (nullptr == m_thread_pool)
        {
            throw BspSocketException("new thread_pool");
        }
        BSP_TRACE_EVENT_END();
    }

    auto accepterCb = [](std::shared_ptr<EventLoop> loop, int fd, std::any args)
    {
        auto server = std::any_cast<std::shared_ptr<TcpServer>>(args);
        server->doAccept();
    };
    m_loop->addIoEvent(m_sockfd, accepterCb, EPOLLIN, shared_from_this());
    m_running.store(true);
    m_loop->processEvents();
    BSP_TRACE_EVENT_END();
}

void TcpServer::stop()
{
    if (!m_running.load())
    {
        return;
    }

    m_thread_pool.reset();
    m_loop->delIoEvent(m_sockfd);
    ::close(m_sockfd);
    ::close(m_reservfd);
    m_running.store(false);
}

void TcpServer::doAccept()
{
    int connfd;
    bool conn_full = false;
    while (true)
    {
        BSP_TRACE_EVENT_BEGIN("tcpserver accept");
        connfd = ::accept(m_sockfd, (struct sockaddr*)&m_conn_addr, &m_addrlen);
        if (connfd == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }
            else if (errno == EMFILE)
            {
                conn_full = true;
                ::close(m_reservfd);
            }
            else if (errno == EAGAIN)
            {
                break;
            }
            else
            {
                throw BspSocketException("accept()");
            }
        }
        else if (conn_full)
        {
            ::close(connfd);
            m_reservfd = ::open("/tmp/reactor_accepter", O_CREAT | O_RDONLY | O_CLOEXEC, 0666);
            if (m_reservfd == -1)
            {
                throw BspSocketException("open()");
            }
        }
        else
        {
            //connfd and max connections
            auto curr_conns = getConnectionNum();

            if (curr_conns >= m_server_params.max_connections)
            {
                m_logger->printStdoutLog(BspLogger::LogLevel::Error, "connection exceeds the maximum connection count {}", m_server_params.max_connections);
                ::close(connfd);
            }
            else
            {
                assert(connfd < m_conns_size);
                if (m_keep_alive.load())
                {
                    int opend = 1;
                    int ret = ::setsockopt(connfd, SOL_SOCKET, SO_KEEPALIVE, &opend, sizeof(opend));
                    if (ret < 0)
                    {
                        throw BspSocketException("setsockopt SO_KEEPALIVE");
                    }
                }

                //multi-thread reactor model: round-robin a event loop and give message to it
                if (m_thread_pool != nullptr)
                {
                    auto cq = m_thread_pool->getNextThread();
                    queueMsg msg;
                    msg.cmd_type = queueMsg::MSG_TYPE::NEW_CONN;
                    msg.connection_fd = connfd;
                    cq->sendMsg(msg);
                }
                else//register in self thread
                {
                    std::weak_ptr<TcpConnection> conn = m_connections_pool[connfd];

                    if (!conn.expired())
                    {
                        conn.lock()->activate(connfd, m_loop);
                    }
                    else
                    {
                        BSP_TRACE_EVENT_BEGIN("tcpserver create connection");
                        m_connections_pool[connfd] = std::make_shared<TcpConnection>(connfd, m_loop, shared_from_this());
                        BSP_TRACE_EVENT_END();
                    }
                }
            }
        }
        BSP_TRACE_EVENT_END();
    }
}

std::weak_ptr<TcpConnection> TcpServer::getConnectionFromPool(int connection_fd)
{
    std::lock_guard<std::mutex> lockGuard(m_mutex);
    return m_connections_pool[connection_fd];
}


void TcpServer::insertConnectionToPool(std::shared_ptr<TcpConnection> conn, int connection_fd)
{
    std::lock_guard<std::mutex> lockGuard(m_mutex);
    m_connections_pool[connection_fd] = conn;
}

void TcpServer::incConnection()
{
    std::lock_guard<std::mutex> lockGuard(m_mutex);
    m_curr_conns++;
}

size_t TcpServer::getConnectionNum()
{
    std::lock_guard<std::mutex> lockGuard(m_mutex);
    return m_curr_conns;
}

void TcpServer::decConnection()
{
    std::lock_guard<std::mutex> lockGuard(m_mutex);
    m_curr_conns--;
}

} // namespace bsp_sockets
