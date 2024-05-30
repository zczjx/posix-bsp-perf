#ifndef __TCP_SERVER_H__
#define __TCP_SERVER_H__

#include "impl/ThreadPool.hpp"
#include "impl/TcpConn.hpp"
#include "EventLoop.hpp"
#include "NetCommu.hpp"

#include "impl/MsgDispatcher.hpp"
#include <shared/BspLogger.hpp>
#include <shared/ArgParser.hpp>

#include <netinet/in.h>
#include <iostream>
#include <mutex>
#include <thread>
#include <functional>
#include <any>

namespace bsp_sockets
{

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
    TcpServer(std::shared_ptr<EventLoop> loop, bsp_perf::shared::ArgParser&& args);

    virtual ~TcpServer();//TcpServer类使用时往往具有程序的完全生命周期，其实并不需要析构函数

    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;
    TcpServer(TcpServer&&) = delete;
    TcpServer& operator=(TcpServer&&) = delete;

    void keepAlive() { m_keep_alive = true; }

    void doAccept();

    void addMsgCallback(int cmdid, msgCallback msg_cb, std::any usr_data) { g_dispatcher.add_msg_cb(cmdid, msg_cb, usr_data); }

    void incConnection();
    size_t getConnectionNum();
    void decConnection();

    std::shared_ptr<EventLoop> loop() { return m_loop; }

    std::shared_ptr<ThreadPool> getThreadPool() { return m_thread_pool; }

public:
    static msg_dispatcher g_dispatcher;
    using conn_callback = std::function<void(net_commu& conn)>;
    static conn_callback connBuildCb{nullptr};//用户设置连接建立后的回调函数
    static conn_callback connCloseCb{nullptr};//用户设置连接释放后的回调函数

    static void onConnBuild(conn_callback cb) { connBuildCb = cb; }
    static void onConnClose(conn_callback cb) { connCloseCb = cb; }

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
    std::unique_ptr<bsp_perf::shared::BspLogger> m_logger;
    bsp_perf::shared::ArgParser m_args;

    std::vector<std::shared_ptr<tcp_conn>> m_connections_pool{};//连接池
};

}

#endif
