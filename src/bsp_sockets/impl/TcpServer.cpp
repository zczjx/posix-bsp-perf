
#include <bsp_sockets/MsgHead.hpp>
#include <bsp_sockets/TcpServer.hpp>
#include <bsp_sockets/BspSocketException.hpp>
#include <bsp_sockets/ConfigReader.hpp>
#include "TcpConn.hpp"
#include "PrintError.hpp"


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


namespace bsp_sockets
{
using namespace bsp_perf::shared;

static void accepterCb(std::shared_ptr<EventLoop>loop, int fd, std::any args)
{
    std::shared_ptr<TcpServer> server = std::any_cast<std::shared_ptr<TcpServer>>(args);
    server->doAccept();
}


TcpServer::TcpServer(std::shared_ptr<EventLoop> loop, const std::string& ip, uint16_t port):
    m_loop(loop),
    m_logger{std::make_unique<BspLogger>()}
{
    m_logger->setPattern();
    bzero(&m_conn_addr, sizeof(m_conn_addr));
    //ignore SIGHUP and SIGPIPE
    if (signal(SIGHUP, SIG_IGN) == SIG_ERR)
    {
        m_logger->printStdoutLog(BspLogger::LogLevel::Error, "signal ignore SIGHUP");
    }
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
    {
        m_logger->printStdoutLog(BspLogger::LogLevel::Error, "signal ignore SIGPIPE");
    }

    //create socket
    m_sockfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);

    if (m_sockfd < 0)
    {
        throw BspSocketException("socket()");
    }

    m_reservfd = open("/tmp/reactor_accepter", O_CREAT | O_RDONLY | O_CLOEXEC, 0666);

    if (m_reservfd < 0)
    {
        throw BspSocketException("open()");
    }

    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;

    int ret = inet_aton(ip.c_str(), &servaddr.sin_addr);

    if (ret == 0)
    {
        throw BspSocketException("ip format");
    }
    servaddr.sin_port = htons(port);

    int opend = 1;
    ret = setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &opend, sizeof(opend));

    if (ret < 0)
    {
        throw BspSocketException("setsockopt SO_REUSEADDR");
    }

    ret = bind(m_sockfd, (const struct sockaddr*)&servaddr, sizeof(servaddr));
    if (ret < 0)
    {
        throw BspSocketException("bind()");
    }

    ret = listen(m_sockfd, 500);

    if (ret < 0)
    {
        throw BspSocketException("listen()");
    }

    info_log("server on %s:%u is running...", ip.c_str(), port);
    m_logger->printStdoutLog(BspLogger::LogLevel::Debug, "server on {}:{} is running...", ip.c_str(), port);
    m_loop->addIoEvent(m_sockfd, accepterCb, EPOLLIN, std::make_any<std::shared_ptr<TcpServer>>(shared_from_this()));
}

{
    ::bzero(&_connaddr, sizeof (_connaddr));
    //ignore SIGHUP and SIGPIPE
    if (::signal(SIGHUP, SIG_IGN) == SIG_ERR)
    {
        error_log("signal ignore SIGHUP");
    }
    if (::signal(SIGPIPE, SIG_IGN) == SIG_ERR)
    {
        error_log("signal ignore SIGPIPE");
    }

    //create socket
    _sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    exit_if(_sockfd == -1, "socket()");

    _reservfd = ::open("/tmp/reactor_accepter", O_CREAT | O_RDONLY | O_CLOEXEC, 0666);
    error_if(_reservfd == -1, "open()");

    struct sockaddr_in servaddr;
    ::bzero(&servaddr, sizeof (servaddr));
    servaddr.sin_family = AF_INET;
    int ret = ::inet_aton(ip, &servaddr.sin_addr);
    exit_if(ret == 0, "ip format %s", ip);
    servaddr.sin_port = htons(port);

    int opend = 1;
    ret = ::setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, &opend, sizeof(opend));
    error_if(ret < 0, "setsockopt SO_REUSEADDR");

    ret = ::bind(_sockfd, (const struct sockaddr*)&servaddr, sizeof servaddr);
    exit_if(ret == -1, "bind()");

    ret = ::listen(_sockfd, 500);
    exit_if(ret == -1, "listen()");

    info_log("server on %s:%u is running...", ip, port);

    _loop = loop;

    _addrlen = sizeof (struct sockaddr_in);

    //if mode is multi-thread reactor, create thread pool
    int thread_cnt = config_reader::ins()->GetNumber("reactor", "threadNum", 0);
    _thd_pool = NULL;
    if (thread_cnt)
    {
        _thd_pool = new thread_pool(thread_cnt);
        exit_if(_thd_pool == NULL, "new thread_pool");
    }

    //create connection pool
    _max_conns = config_reader::ins()->GetNumber("reactor", "maxConns", 10000);
    int next_fd = ::dup(1);
    _conns_size = _max_conns + next_fd;
    ::close(next_fd);

    conns = new tcp_conn*[_conns_size];
    exit_if(conns == NULL, "new conns[%d]", _conns_size);
    for (int i = 0;i < _max_conns + next_fd; ++i)
        conns[i] = NULL;

    //add accepter event
    _loop->add_ioev(_sockfd, accepter_cb, EPOLLIN, this);
}

//tcp_server类使用时往往具有程序的完全生命周期，其实并不需要析构函数
tcp_server::~tcp_server()
{
    _loop->del_ioev(_sockfd);
    ::close(_sockfd);
    ::close(_reservfd);
}

void TcpServer::doAccept()
{
    int connfd;
    bool conn_full = false;
    while (true)
    {
        connfd = ::accept(_sockfd, (struct sockaddr*)&_connaddr, &_addrlen);
        if (connfd == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }
            else if (errno == EMFILE)
            {
                conn_full = true;
                ::close(_reservfd);
            }
            else if (errno == EAGAIN)
            {
                break;
            }
            else
            {
                exit_log("accept()");
            }
        }
        else if (conn_full)
        {
            ::close(connfd);
            _reservfd = ::open("/tmp/reactor_accepter", O_CREAT | O_RDONLY | O_CLOEXEC, 0666);
            error_if(_reservfd == -1, "open()");
        }
        else
        {
            //connfd and max connections
            int curr_conns;
            get_conn_num(curr_conns);
            if (curr_conns >= _max_conns)
            {
                error_log("connection exceeds the maximum connection count %d", _max_conns);
                ::close(connfd);
            }
            else
            {
                assert(connfd < _conns_size);
                if (_keepalive)
                {
                    int opend = 1;
                    int ret = ::setsockopt(connfd, SOL_SOCKET, SO_KEEPALIVE, &opend, sizeof(opend));
                    error_if(ret < 0, "setsockopt SO_KEEPALIVE");
                }

                //multi-thread reactor model: round-robin a event loop and give message to it
                if (_thd_pool)
                {
                    thread_queue<queue_msg>* cq = _thd_pool->get_next_thread();
                    queue_msg msg;
                    msg.cmd_type = queue_msg::NEW_CONN;
                    msg.connfd = connfd;
                    cq->send_msg(msg);
                }
                else//register in self thread
                {
                    tcp_conn* conn = conns[connfd];
                    if (conn)
                    {
                        conn->init(connfd, _loop);
                    }
                    else
                    {
                        conn = new tcp_conn(connfd, _loop);
                        exit_if(conn == NULL, "new tcp_conn");
                        conns[connfd] = conn;
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


}
