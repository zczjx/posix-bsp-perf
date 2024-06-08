#ifndef __TCP_SERVER_H__
#define __TCP_SERVER_H__

#include "impl/ThreadPool.hpp"
#include "impl/TcpConnection.hpp"
#include "EventLoop.hpp"
#include "ISocketConnection.hpp"

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

namespace bsp_sockets
{

using bsp_perf::shared;
using ServerParams = struct ServerParams
{
    std::string ipaddr{""};
    int port{-1};
    int thread_num{0};
    int max_connections{0};
};
class TcpServer
{
public:
    TcpServer(std::shared_ptr<EventLoop> loop, ArgParser&& args);

    virtual ~TcpServer();//TcpServer类使用时往往具有程序的完全生命周期，其实并不需要析构函数

    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;
    TcpServer(TcpServer&&) = delete;
    TcpServer& operator=(TcpServer&&) = delete;

    void keepAlive() { m_keep_alive = true; }

    void doAccept();

    void addMsgCallback(int cmd_id, msgCallback msg_cb, std::any usr_data) { m_msg_dispatcher.addMsgCallback(cmd_id, msg_cb, usr_data); }

    void incConnection();

    void decConnection();

    size_t getConnectionNum();

    std::weak_ptr<tcp_conn> getConnectionFromPool(int connection_fd);

    void insertConnectionToPool(std::shared_ptr<tcp_conn> conn, int connection_fd);

    std::shared_ptr<EventLoop> loop() { return m_loop; }

    std::shared_ptr<ThreadPool> getThreadPool() { return m_thread_pool; }

public:
    MsgDispatcher& getMsgDispatcher() { return m_msg_dispatcher; }
    using connectionCallback = std::function<void(ISocketConnection& conn)>;
    connectionCallback connectionEstablishCb{nullptr};//用户设置连接建立后的回调函数
    connectionCallback connectionCloseCb{nullptr};//用户设置连接释放后的回调函数
    void onConnectionEstablish(connectionCallback cb) { connBuildCb = cb; }
    void onConnectionClose(connectionCallback cb) { connCloseCb = cb; }

private:
    ServerParams m_server_params;
    int m_sockfd{-1};
    int m_reservfd{-1};
    std::shared_ptr<EventLoop> m_loop{nullptr};
    std::shared_ptr<ThreadPool> m_thread_pool{nullptr};
    struct sockaddr_in m_conn_addr{};
    socklen_t m_addrlen{sizeof(struct sockaddr_in)};
    bool m_keep_alive{false};

    int m_conns_size{0};
    int m_curr_conns{0};
    std::mutex m_mutex{}; // Default initialization
    std::unique_ptr<BspLogger> m_logger;
    ArgParser m_args;

    std::vector<std::shared_ptr<tcp_conn>> m_connections_pool{};//连接池
    MsgDispatcher m_msg_dispatcher{};
};

}

#endif
