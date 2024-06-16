#ifndef __UDP_SERVER_HPP__
#define __UDP_SERVER_HPP__

#include "impl/ISocketHelper.hpp"
#include "impl/MsgDispatcher.hpp"

#include "EventLoop.hpp"
#include <shared/BspLogger.hpp>
#include <shared/ArgParser.hpp>

#include <netinet/in.h>

#include <any>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace bsp_sockets
{

using namespace bsp_perf::shared;
using UdpServerParams = struct UdpServerParams
{
    std::string ip_addr{""};
    int port{-1};
};

class UdpServer: public ISocketHelper, public std::enable_shared_from_this<UdpServer>
{
public:
    UdpServer(std::shared_ptr<EventLoop> loop, ArgParser&& args);

    virtual ~UdpServer();

    UdpServer(const UdpServer&) = delete;
    UdpServer& operator=(const UdpServer&) = delete;
    UdpServer(UdpServer&&) = delete;
    UdpServer& operator=(UdpServer&&) = delete;

    void addMsgCallback(int cmd_id, msgCallback msg_cb, std::any usr_data) { m_msg_dispatcher.addMsgCallback(cmd_id, msg_cb, usr_data); }

    std::shared_ptr<EventLoop> getEventLoop() { return m_loop; }

    void handleRead();

    virtual int sendData(std::vector<uint8_t>& data, int cmd_id) override;

    virtual int getFd() override { return m_sockfd; }

private:
    UdpServerParams m_server_params{};
    std::unique_ptr<BspLogger> m_logger;
    ArgParser m_args;

    int m_sockfd;
    std::shared_ptr<EventLoop> m_loop{nullptr};

    struct sockaddr_in m_src_addr{};
    socklen_t m_addrlen{sizeof(struct sockaddr_in)};

    MsgDispatcher m_msg_dispatcher{};

    std::vector<uint8_t> m_rbuf{};
    std::vector<uint8_t> m_wbuf{};
};

} // namespace bsp_sockets

#endif
