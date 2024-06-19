#include<bsp_sockets/TcpServer.hpp>
#include<bsp_sockets/EventLoop.hpp>
#include <shared/BspLogger.hpp>

using namespace bsp_perf::shared;
using namespace bsp_sockets;

static void onMessage(std::vector<uint8_t>& data, int cmd_id, std::shared_ptr<ISocketHelper> socket_helper, std::any usr_data)
{
    auto logger = std::any_cast<std::shared_ptr<BspLogger>>(usr_data);
    std::string data_str(data.begin(), data.end());
    logger->printStdoutLog(BspLogger::LogLevel::Critical, "TCP Server onMessage: cmd_id=0x{0:x}, data={1:s}", cmd_id, data_str);

    socket_helper->sendData(data, cmd_id);
}

int main(int argc, char* argv[])
{
    ArgParser parser("Asyncio Sockets Perf Case: Tcp Server");
    parser.addOption("--ip", std::string("127.0.0.1"), "tcp server ip address");
    parser.addOption("--port", int32_t(12345), "port number for the tcp server");
    parser.addOption("--thread_num", int32_t(10), "thread number for the tcp server");
    parser.addOption("--max_connections", int32_t(10), "thread number for the tcp server");
    parser.parseArgs(argc, argv);

    std::shared_ptr<EventLoop> loop_ptr = std::make_shared<EventLoop>();

    std::shared_ptr<TcpServer> server = std::make_shared<TcpServer>(loop_ptr, std::move(parser));
    server->start();
    std::shared_ptr<BspLogger> logger = std::make_shared<BspLogger>("aio_tcpserver");
    server->addMsgCallback(1, onMessage, logger);
    loop_ptr->processEvents();

    return 0;
}