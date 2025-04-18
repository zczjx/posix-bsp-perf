#include <iostream>
#include <string>
#include <shared/ArgParser.hpp>
#include <zeromq_ipc/zmqSubscriber.hpp>

using namespace midware::zeromq_ipc;
using namespace bsp_perf::shared;

int main(int argc, char* argv[])
{
    ArgParser parser("Zmq Pub Sub");
    parser.addOption("--topic", std::string("tcp://127.0.0.1:15555"), "topic for the zmq pub sub");
    parser.parseArgs(argc, argv);

    std::string topic;
    parser.getOptionVal("--topic", topic);

    ZmqSubscriber subscriber(topic);
    while (true)
    {
        std::vector<uint8_t> data;
        subscriber.receiveData(data);
        std::cout << "Received data: " << std::string(data.begin(), data.end()) << std::endl;
    }

    return 0;
}