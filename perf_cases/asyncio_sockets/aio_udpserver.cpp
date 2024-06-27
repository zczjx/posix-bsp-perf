#include<bsp_sockets/UdpServer.hpp>
#include<bsp_sockets/EventLoop.hpp>
#include <shared/BspLogger.hpp>
#include <iostream>

using namespace bsp_perf::shared;
using namespace bsp_sockets;

static void onMessage(std::vector<uint8_t>& data, int cmd_id, std::shared_ptr<ISocketHelper> socket_helper, std::any usr_data)
{
    auto logger = std::any_cast<std::shared_ptr<BspLogger>>(usr_data);
    std::string data_str(data.begin(), data.end());
    logger->printStdoutLog(BspLogger::LogLevel::Warn, "TCP Server onMessage: cmd_id=0x{0:04x}, data={1:s}", cmd_id, data_str);
    data_str += " from server";
    data.assign(data_str.begin(), data_str.end());

    socket_helper->sendData(data, cmd_id);
}

int main(int argc, char* argv[])
{
    ArgParser parser("Asyncio Sockets Perf Case: UDP Server");
    parser.addOption("--ip", std::string("127.0.0.1"), "tcp server ip address");
    parser.addOption("--port", int32_t(12347), "port number for the udp server");
    parser.parseArgs(argc, argv);

    std::shared_ptr<EventLoop> loop_ptr = std::make_shared<EventLoop>();

    std::shared_ptr<UdpServer> server = std::make_shared<UdpServer>(loop_ptr, std::move(parser));
    std::shared_ptr<BspLogger> logger = std::make_shared<BspLogger>("aio_udpserver");
    server->addMsgCallback(1, onMessage, logger);
    server->startLoop();

    return 0;
}