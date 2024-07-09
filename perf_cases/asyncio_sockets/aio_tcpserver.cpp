#include <bsp_sockets/TcpServer.hpp>
#include <bsp_sockets/EventLoop.hpp>
#include <shared/BspLogger.hpp>
#include <iostream>
#include <profiler/BspTrace.hpp>

using namespace bsp_perf::shared;
using namespace bsp_sockets;
using namespace bsp_perf::common;

static void onMessage(size_t cmd_id, std::vector<uint8_t>& data, std::shared_ptr<ISocketHelper> socket_helper, std::any usr_data)
{
    BSP_TRACE_EVENT_BEGIN("aio tcpserver onMessage");
    auto logger = std::any_cast<std::shared_ptr<BspLogger>>(usr_data);
    std::string data_str(data.begin(), data.end());
    logger->printStdoutLog(BspLogger::LogLevel::Warn, "TCP Server onMessage: cmd_id=0x{0:04x}, data={1:s}", cmd_id, data_str);
    data_str += " from server";
    data.assign(data_str.begin(), data_str.end());

    socket_helper->sendData(cmd_id, data);
    BSP_TRACE_EVENT_END();
}

int main(int argc, char* argv[])
{
    ArgParser parser("Asyncio Sockets Perf Case: Tcp Server");
    parser.addOption("--ip", std::string("127.0.0.1"), "tcp server ip address");
    parser.addOption("--port", int32_t(12345), "port number for the tcp server");
    parser.addOption("--thread_num", int32_t(10), "thread number for the tcp server");
    parser.addOption("--max_connections", int32_t(1024), "thread number for the tcp server");
    parser.parseArgs(argc, argv);

    BspTrace ddrPerfTrace("./aio_tcpserver.perfetto");

    BSP_TRACE_EVENT_BEGIN("aio tcpserver");
    std::shared_ptr<EventLoop> loop_ptr = std::make_shared<EventLoop>();

    BSP_TRACE_EVENT_BEGIN("aio tcpserver create");
    std::shared_ptr<TcpServer> server = std::make_shared<TcpServer>(loop_ptr, std::move(parser));
    BSP_TRACE_EVENT_END();
    std::shared_ptr<BspLogger> logger = std::make_shared<BspLogger>("aio_tcpserver");
    std::string cmd_name("aio tcp perf case");
    server->addMsgCallback(cmd_name, onMessage, logger);
    server->startLoop();
    BSP_TRACE_EVENT_END();
    return 0;
}