
#include <bsp_sockets/MsgHead.hpp>
#include <bsp_sockets/TcpServer.hpp>
#include <bsp_sockets/BspSocketException.hpp>
#include <bsp_sockets/ConfigReader.hpp>
#include "TcpConn.hpp"


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

namespace bsp_sockets
{
using namespace bsp_perf::shared;

static void accepterCb(std::shared_ptr<EventLoop>loop, int fd, std::any args)
{
    std::shared_ptr<TcpServer> server = std::any_cast<std::shared_ptr<TcpServer>>(args);
    server->doAccept();
}


TcpServer::TcpServer(std::shared_ptr<EventLoop> loop, bsp_perf::shared::ArgParser&& args):
    m_loop(loop),
    m_args{std::move(args)},
    m_logger{std::make_unique<BspLogger>()}
{
    m_args.getOptionVal("--ip", m_server_params.ipaddr);
    m_args.getOptionVal("--port", m_server_params.port);
    m_args.getOptionVal("--threadNum", m_server_params.thread_num);
    m_args.getOptionVal("--maxConns", m_server_params.max_connections);

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
    m_sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);

    if (m_sockfd < 0)
    {
        throw BspSocketException("socket()");
    }

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

    info_log("server on %s:%u is running...", m_server_params.ipaddr.c_str(), m_server_params.port);
    m_logger->printStdoutLog(BspLogger::LogLevel::Debug, "server on {}:{} is running...", m_server_params.ipaddr.c_str(), m_server_params.port);

    if (m_server_params.thread_num > 0)
    {
        m_thd_pool = std::make_shared<ThreadPool>(m_server_params.thread_num);
        if (nullptr == m_thd_pool)
        {
            throw BspSocketException("new thread_pool");
        }
    }

    int next_fd = ::dup(1);
    m_conns_size = m_server_params.max_connections + next_fd;
    ::close(next_fd);
    m_connections_pool.resize(m_conns_size);
    for (auto& conn : m_connections_pool)
    {
        conn.reset(); // 或者 conn = nullptr;
    }

    m_loop->addIoEvent(m_sockfd, accepterCb, EPOLLIN, std::make_any<std::shared_ptr<TcpServer>>(shared_from_this()));
}

//TcpServer类使用时往往具有程序的完全生命周期，其实并不需要析构函数
TcpServer::~TcpServer()
{
    m_loop->delIoEvent(m_sockfd);
    ::close(m_sockfd);
    ::close(m_reservfd);

    // Clean up the connections
    for (auto& conn : m_connections_pool)
    {
        conn.reset();
    }
}

void TcpServer::doAccept()
{
    int connfd;
    bool conn_full = false;
    while (true)
    {
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
            int curr_conns;
            get_conn_num(curr_conns);
            if (curr_conns >= m_server_params.max_connections)
            {
                m_logger->printStdoutLog(BspLogger::LogLevel::Error, "connection exceeds the maximum connection count {}", m_server_params.max_connections);
                ::close(connfd);
            }
            else
            {
                assert(connfd < m_conns_size);
                if (m_keep_alive)
                {
                    int opend = 1;
                    int ret = ::setsockopt(connfd, SOL_SOCKET, SO_KEEPALIVE, &opend, sizeof(opend));
                    if (ret < 0)
                    {
                        throw BspSocketException("setsockopt SO_KEEPALIVE");
                    }
                }

                //multi-thread reactor model: round-robin a event loop and give message to it
                if (m_thd_pool != nullptr)
                {
                    thread_queue<queue_msg>* cq = m_thd_pool->get_next_thread();
                    queue_msg msg;
                    msg.cmd_type = queue_msg::NEW_CONN;
                    msg.connfd = connfd;
                    cq->send_msg(msg);
                }
                else//register in self thread
                {

                    std::weak_ptr<tcp_conn> conn = m_connections_pool[connfd];
                    if (!conn.expired())
                    {
                        conn->init(connfd, m_loop);
                    }
                    else
                    {
                        m_connections_pool[connfd] = std::shared_ptr<tcp_conn>(connfd, m_loop);
                    }
                }
            }
        }
    }
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
