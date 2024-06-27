#ifndef __UDP_CLIENT_H__
#define __UDP_CLIENT_H__

#include "impl/ISocketHelper.hpp"
#include "impl/MsgDispatcher.hpp"
#include "EventLoop.hpp"
#include <shared/BspLogger.hpp>
#include <shared/ArgParser.hpp>

#include <netinet/in.h>
#include <atomic>

namespace bsp_sockets
{
using namespace bsp_perf::shared;

using UdpClientParams = struct UdpClientParams
{
    std::string ip_addr{""};
    int port{-1};
};

class UdpClient: public ISocketHelper, public std::enable_shared_from_this<UdpClient>
{
public:
    UdpClient(std::shared_ptr<EventLoop> loop, ArgParser&& args);

    virtual ~UdpClient();

    UdpClient(const UdpClient&) = delete;
    UdpClient& operator=(const UdpClient&) = delete;
    UdpClient(UdpClient&&) = delete;
    UdpClient& operator=(UdpClient&&) = delete;
    using onConnectCallback = std::function<void(std::shared_ptr<UdpClient> client)>;

    void startLoop();

    void stop();

    UdpClientParams& getUdpClientParams() { return m_client_params; }

    void addMsgCallback(int cmd_id, msgCallback msg_cb, std::any usr_data) { m_msg_dispatcher.addMsgCallback(cmd_id, msg_cb, usr_data); }

    std::shared_ptr<EventLoop> getEventLoop() { return m_loop; }

    void handleRead();

    void setOnConnectCallback(onConnectCallback cb) { onConnectFunc = cb; }

    virtual int sendData(std::vector<uint8_t>& data, int cmd_id) override;

    virtual int getFd() override { return m_sockfd; }

private:
    onConnectCallback onConnectFunc{nullptr};//用户设置连接建立后的回调函数

private:
    UdpClientParams m_client_params{};
    std::unique_ptr<BspLogger> m_logger{nullptr};
    ArgParser m_args;

    int m_sockfd{-1};
    std::vector<uint8_t> m_rbuf{};
    std::vector<uint8_t> m_wbuf{};
    std::shared_ptr<EventLoop> m_loop{nullptr};
    MsgDispatcher m_msg_dispatcher{};

    std::atomic_bool m_running{false};
};

} // namespace bsp_sockets

#endif
