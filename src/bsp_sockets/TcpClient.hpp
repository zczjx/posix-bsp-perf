#ifndef __TCP_CLIENT_H__
#define __TCP_CLIENT_H__

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "EventLoop.hpp"
#include "ISocketConnection.hpp"
#include "impl/IOBuffer.hpp"
#include "impl/MsgDispatcher.hpp"


#include <shared/BspLogger.hpp>
#include <shared/ArgParser.hpp>

#include <any>
#include <functional>
#include <memory>

namespace bsp_sockets
{

using bsp_perf::shared;

using ClientParams = struct ClientParams
{
    std::string ip_addr{""};
    int port{-1};
    std::string name{""};
};
class TcpClient: public ISocketConnection, public std::enable_shared_from_this<TcpClient>
{
public:
    TcpClient(std::shared_ptr<EventLoop> loop, ArgParser&& args);
    virtual ~TcpClient() { ::close(_sockfd); }

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

    void onConnect()
    {
        if (m_on_connection_func)
        {
            m_on_connection_func(shared_from_this(), m_on_connection_args);
        }
    }

    void onClose()
    {
        if (m_on_close_func)
        {
            m_on_close_func(shared_from_this(), m_on_close_args);
        }
    }

    void addMsgCallback(int cmd_id, msgCallback msg_cb, std::any usr_data)
    { m_msg_dispatcher.addMsgCallback(cmd_id, msg_cb, usr_data); }

    void doConnect();

    int sendData(std::span<const uint8_t> data, int datlen, int cmd_id) override;

    int getFd() override { return m_sockfd; }

    int handleRead();

    int handleWrite();

    void cleanConnection();

    std::shared_ptr<EventLoop> getEventLoop() { return m_loop; }


private:
    struct sockaddr_in m_server_addr;
    bool m_net_ok{false};

    ClientParams m_client_params;
    int m_sockfd;
    std::shared_ptr<EventLoop> m_loop{nullptr};
    socklen_t m_addrlen{sizeof(struct sockaddr_in)};
    MsgDispatcher m_msg_dispatcher{};
    //when connection success, call _onconnection(_onconn_args)
    onConnectFunc m_on_connection_func{nullptr};
    std::any m_on_connection_args{nullptr};
    //when connection close, call _onclose(_onclose_args)
    onCloseFunc m_on_close_func{nullptr};
    std::any m_on_close_args{nullptr};

    IOBufferQueue m_inbuf_queue{};
    IOBufferQueue m_outbuf_queue{};

    std::unique_ptr<BspLogger> m_logger;
    ArgParser m_args;
};
}



#endif
