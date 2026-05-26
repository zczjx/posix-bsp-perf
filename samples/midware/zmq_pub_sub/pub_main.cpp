#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <shared/ArgParser.hpp>
#include <zeromq_ipc/zmqPublisher.hpp>

using namespace midware::zeromq_ipc;
using namespace bsp_perf::shared;

int main(int argc, char* argv[])
{
    ArgParser parser("Zmq Pub Sub");
    parser.addOption("--topic", std::string("tcp://127.0.0.1:15555"), "topic for the zmq pub sub");
    parser.addOption("--msg", std::string("Hello World"), "message to send");
    parser.parseArgs(argc, argv);

    std::string topic;
    parser.getOptionVal("--topic", topic);

    std::string msg;
    parser.getOptionVal("--msg", msg);

    ZmqPublisher publisher(topic);

    while (true)
    {
        std::cout << "Sending message: " << msg << std::endl;
        publisher.sendData(msg.data(), msg.size());
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}