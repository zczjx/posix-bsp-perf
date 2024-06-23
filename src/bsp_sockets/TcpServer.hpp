#ifndef __TCP_SERVER_HPP__
#define __TCP_SERVER_HPP__

#include "impl/ThreadPool.hpp"
#include "impl/TcpConnection.hpp"
#include "impl/ISocketHelper.hpp"
#include "EventLoop.hpp"

#include "impl/MsgDispatcher.hpp"
#include <shared/BspLogger.hpp>
#include <shared/ArgParser.hpp>

#include <netinet/in.h>
#include <iostream>
#include <mutex>
#include <thread>
#include <functional>
#include <memory>
#include <any>
#include <atomic>

namespace bsp_sockets
{

using namespace bsp_perf::shared;
using TcpServerParams = struct TcpServerParams
{
    std::string ipaddr{""};
    int port{-1};
    int thread_num{0};
    int max_connections{0};
};
class TcpServer: public std::enable_shared_from_this<TcpServer>
{
public:
    TcpServer(std::shared_ptr<EventLoop> loop, ArgParser&& args);

    virtual ~TcpServer();//TcpServer类使用时往往具有程序的完全生命周期，其实并不需要析构函数

    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;
    TcpServer(TcpServer&&) = delete;
    TcpServer& operator=(TcpServer&&) = delete;

    void startLoop();

    void stop();

    void keepAlive() { m_keep_alive.store(true); }

    TcpServerParams& getTcpServerParams() { return m_server_params; }

    void doAccept();

    void addMsgCallback(int cmd_id, msgCallback msg_cb, std::any usr_data) { m_msg_dispatcher.addMsgCallback(cmd_id, msg_cb, usr_data); }

    void incConnection();

    void decConnection();

    size_t getConnectionNum();

    std::weak_ptr<TcpConnection> getConnectionFromPool(int connection_fd);

    void insertConnectionToPool(std::shared_ptr<TcpConnection> conn, int connection_fd);

    std::shared_ptr<EventLoop> getEventLoop() { return m_loop; }
    std::shared_ptr<ThreadPool> getThreadPool() { return m_thread_pool; }

public:
    MsgDispatcher& getMsgDispatcher() { return m_msg_dispatcher; }
    using connectionCallback = std::function<void(std::shared_ptr<ISocketHelper> conn)>;
    void setConnectionEstablishCallback(connectionCallback cb) { onConnectionEstablish = cb; }
    void setConnectionCloseCallback(connectionCallback cb) { onConnectionClose = cb; }
    connectionCallback onConnectionEstablish{nullptr};//用户设置连接建立后的回调函数
    connectionCallback onConnectionClose{nullptr};//用户设置连接释放后的回调函数

private:

    TcpServerParams m_server_params;
    int m_sockfd{-1};
    int m_reservfd{-1};
    std::shared_ptr<EventLoop> m_loop{nullptr};
    std::shared_ptr<ThreadPool> m_thread_pool{nullptr};
    struct sockaddr_in m_conn_addr{};
    socklen_t m_addrlen{sizeof(struct sockaddr_in)};
    std::atomic_bool m_keep_alive{false};
    std::atomic_bool m_running{false};

    int m_conns_size{0};
    int m_curr_conns{0};
    std::mutex m_mutex{}; // Default initialization
    std::unique_ptr<BspLogger> m_logger;
    ArgParser m_args;

    std::vector<std::shared_ptr<TcpConnection>> m_connections_pool{};//连接池
    MsgDispatcher m_msg_dispatcher{};
};

}

#endif // __TCP_SERVER_HPP__
