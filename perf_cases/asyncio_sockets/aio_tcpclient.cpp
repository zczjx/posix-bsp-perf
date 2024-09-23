#include <bsp_sockets/EventLoop.hpp>
#include <bsp_sockets/TcpClient.hpp>
#include <shared/BspLogger.hpp>
#include <shared/ArgParser.hpp>

#include <thread>
#include <vector>
#include <string>
#include <memory>
#include <utility>
#include <any>
#include <iostream>
#include <chrono>
#include <sstream>

using namespace bsp_perf::shared;
using namespace bsp_sockets;

struct testQPS
{
    testQPS(): lstTs(0), succ(0) {}
    long lstTs;
    int succ;
};

static void onMessage(size_t cmd_id, std::vector<uint8_t>& data, std::shared_ptr<ISocketHelper> socket_helper, std::any usr_data)
{
    auto pair_args = std::any_cast<std::pair<std::shared_ptr<BspLogger>, std::shared_ptr<struct testQPS>>>(usr_data);
    auto logger = pair_args.first;
    auto qps = pair_args.second;
    qps->succ++;
    long curTs = time(NULL);

    if (curTs - qps->lstTs >= 1)
    {
        logger->printStdoutLog(BspLogger::LogLevel::Warn, "** qps:{0:d} **", qps->succ);
        qps->lstTs = curTs;
    }
    std::string data_str(data.begin(), data.end());
    logger->printStdoutLog(BspLogger::LogLevel::Warn, "client: data={0:s}", data_str);

    std::ostringstream str_convert;
    str_convert << "I miss you data" <<" index: "<< qps->succ  << ":" << qps->lstTs
        << "Thread ID: " << std::this_thread::get_id();

    std::string reqStr = str_convert.str();
    std::vector<uint8_t> data_buffer(reqStr.begin(), reqStr.end());
    socket_helper->sendData(cmd_id, data_buffer);
}

void onConnection(std::shared_ptr<TcpClient> client, std::any args)
{
    std::string cmd_name = std::any_cast<std::string>(args);
    std::cout << "[S] zczjx--> aio_tcpclient:onConnection" << std::endl;
    std::string reqStr = "zczjx--> aio client onConnection";
    std::vector<uint8_t> data_buffer(reqStr.begin(), reqStr.end());
    client->sendData(cmd_name, data_buffer); //主动发送消息
    std::cout << "[E] zczjx--> aio_tcpclient:onConnection" << std::endl;
}

void domain(int argc, char* argv[])
{
    ArgParser parser("Asyncio Sockets Perf Case: Tcp Client Thread");
    parser.addOption("--server_ip", std::string("127.0.0.1"), "tcp server ip address");
    parser.addOption("--server_port", int32_t(12345), "port number for the tcp server");
    parser.addOption("--name", std::string("aio_tcpclient"), "name of the tcp client");
    parser.addOption("--thread_num", int32_t(30), "thread number for the tcp server");
    parser.addOption("--poll", std::string("epoll"), "choose Poll or Epoll for Eventloop");
    parser.parseArgs(argc, argv);

    std::string Poll_flag{};
    parser.getOptionVal("--poll", Poll_flag);

    auto loop_ptr = bsp_sockets::EventLoop::create(Poll_flag);

    std::shared_ptr<TcpClient> client = std::make_shared<TcpClient>(loop_ptr, std::move(parser));

    std::shared_ptr<struct testQPS> qps_ptr = std::make_shared<struct testQPS>();

    std::shared_ptr<BspLogger> logger = std::make_shared<BspLogger>(std::string("client"));

    std::string cmd_name("aio tcp perf case");

    client->addMsgCallback(cmd_name, onMessage, std::make_pair(logger, qps_ptr)); //设置：当收到消息id=1的消息时的回调函数

    //安装连接建立后调用的回调函数
    client->setOnConnection(onConnection, cmd_name);

    client->startLoop();
}


int main(int argc, char* argv[])
{
    ArgParser parser("Asyncio Sockets Perf Case: Tcp Server");
    parser.addOption("--thread_num", int32_t(10), "thread number for the tcp server");

    parser.parseArgs(argc, argv);

    int thread_num = 0;
    parser.getOptionVal("--thread_num", thread_num);

    std::vector<std::thread> threads(thread_num);

    for (int i = 0; i < thread_num; i++)
    {
        threads[i] = std::thread(domain, argc, argv);
    }

    for (int i = 0; i < thread_num; i++)
    {
        threads[i].join();
    }


    return 0;
}