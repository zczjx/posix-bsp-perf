#ifndef __UDP_CLIENT_H__
#define __UDP_CLIENT_H__

#include "ISocketConnection.hpp"
#include "impl/MsgDispatcher.hpp"
#include "EventLoop.hpp"
#include <shared/BspLogger.hpp>
#include <shared/ArgParser.hpp>

#include <netinet/in.h>

namespace bsp_socket
{
using bsp_perf::shared;

using UdpClientParams = struct UdpClientParams
{
    std::string ipaddr{""};
    int port{-1};
};

class UdpClient: public ISocketConnection, public std::enable_shared_from_this<UdpClient>
{
public:
    UdpClient(std::shared_ptr<EventLoop> loop, ArgParser&& args);

    virtual ~UdpClient();

    UdpClient(const UdpClient&) = delete;
    UdpClient& operator=(const UdpClient&) = delete;
    UdpClient(UdpClient&&) = delete;
    UdpClient& operator=(UdpClient&&) = delete;

    void addMsgCallback(int cmd_id, msgCallback msg_cb, std::any usr_data) { m_msg_dispatcher.addMsgCallback(cmd_id, msg_cb, usr_data); }

    std::shared_ptr<EventLoop> getEventLoop() { return m_loop; }

    void handleRead();

    virtual int sendData(std::span<const uint8_t> data, int datlen, int cmd_id) override;

    virtual int getFd() override { return m_sockfd; }

private:
    std::unique_ptr<BspLogger> m_logger;
    ArgParser m_args;

    int m_sockfd;
    std::vector<uint8_t> m_rbuf{};
    std::vector<uint8_t> m_wbuf{};
    std::shared_ptr<EventLoop> m_loop{nullptr};
    MsgDispatcher m_msg_dispatcher{};
};

} // namespace bsp_socket

#endif
