#ifndef __TCP_CLIENT_H__
#define __TCP_CLIENT_H__

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "EventLoop.hpp"
#include "impl/ISocketHelper.hpp"
#include "impl/IOBuffer.hpp"
#include "impl/MsgDispatcher.hpp"


#include <shared/BspLogger.hpp>
#include <shared/ArgParser.hpp>

#include <any>
#include <functional>
#include <memory>

namespace bsp_sockets
{

using namespace bsp_perf::shared;
using TcpClientParams = struct TcpClientParams
{
    std::string ip_addr{""};
    int port{-1};
    std::string name{""};
};
class TcpClient: public ISocketHelper, public std::enable_shared_from_this<TcpClient>
{
public:
    TcpClient(std::shared_ptr<EventLoop> loop, ArgParser&& args);
    virtual ~TcpClient() { ::close(m_sockfd); }

    TcpClient(const TcpClient&) = delete;
    TcpClient& operator=(const TcpClient&) = delete;
    TcpClient(TcpClient&&) = delete;
    TcpClient& operator=(TcpClient&&) = delete;

    using onConnectFunc = std::function<void(std::shared_ptr<TcpClient> client, std::any args)>;
    using onCloseFunc = std::function<void(std::shared_ptr<TcpClient> client, std::any args)>;

    //set up function after connection ok
    void setOnConnection(onConnectFunc func, std::any args = nullptr)
    {
        m_on_connection_func = func;
        m_on_connection_args = args;
    }

    //set up function after connection closed
    void setOnClose(onCloseFunc func, std::any args = nullptr)
    {
        m_on_close_func = func;
        m_on_close_args = args;
    }

    int start();

    void stop();
    void onConnect();

    void onClose();

    void addMsgCallback(int cmd_id, msgCallback msg_cb, std::any usr_data)
    { m_msg_dispatcher.addMsgCallback(cmd_id, msg_cb, usr_data); }

    int sendData(std::vector<uint8_t>& data, int cmd_id) override;

    int getFd() override { return m_sockfd; }

    int handleRead();

    int handleWrite();

    void cleanConnection();

    void doConnect();

    bool isOutputBufferEmpty() { return m_outbuf_queue.isEmpty(); }

    void setNetConnected(bool connected) { m_net_connected = connected; }

    std::shared_ptr<EventLoop> getEventLoop() { return m_loop; }

private:
    TcpClientParams m_client_params;
    struct sockaddr_in m_remote_server_addr;
    socklen_t m_addrlen{sizeof(struct sockaddr_in)};
    bool m_net_connected{false};
    int m_sockfd;

    std::shared_ptr<EventLoop> m_loop{nullptr};
    MsgDispatcher m_msg_dispatcher{};
    //when connection success, call _onconnection(_onconn_args)
    onConnectFunc m_on_connection_func{nullptr};
    std::any m_on_connection_args{nullptr};
    //when connection close, call _onclose(_onclose_args)
    onCloseFunc m_on_close_func{nullptr};
    std::any m_on_close_args{nullptr};

    OutputBufferQueue m_outbuf_queue{};

    std::unique_ptr<BspLogger> m_logger;
    ArgParser m_args;
};
}



#endif
