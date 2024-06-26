#include <bsp_sockets/EventLoop.hpp>
#include <shared/BspLogger.hpp>
#include <shared/ArgParser.hpp>
#include <iostream>
#include <vector>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace bsp_perf::shared;
using namespace bsp_sockets;

static int createNonblockingUDP()
{
  int sockfd = ::socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_UDP);
  if (sockfd < 0)
  {
    std::cout << "::socket failed" << std::endl;
  }
  return sockfd;
}

static void readCallback(std::shared_ptr<EventLoop> loop, int sockfd, std::any args)
{
    std::vector<uint8_t> rbuf(1024);

    ssize_t nr = ::recv(sockfd, rbuf.data(), rbuf.size(), 0);
    if (nr < 0)
    {
        std::cout << "::recv failed" << std::endl;
    }

    std::cout << "received: " << nr << "bytes from server" << std::endl;
    std::string data_str(rbuf.begin(), rbuf.end());
    std::cout << "data: " << data_str << std::endl;

    size_t nw = ::send(sockfd, rbuf.data(), nr, 0);
    if (nw < 0)
    {
        std::cout << "::send failed" << std::endl;
    }
}


int main(int argc, char* argv[])
{
    ArgParser parser("Asyncio Sockets Perf Case: UDP Server");
    parser.addOption("--ip", std::string("127.0.0.1"), "tcp server ip address");
    parser.addOption("--port", int32_t(12347), "port number for the udp server");
    parser.parseArgs(argc, argv);

    std::shared_ptr<EventLoop> loop_ptr = std::make_shared<EventLoop>();

    std::string ip_addr{""};
    int port{-1};

    parser.getOptionVal("--ip", ip_addr);
    parser.getOptionVal("--port", port);

    int sockfd = createNonblockingUDP();

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    ::inet_aton(ip_addr.c_str(), &servaddr.sin_addr);
    // servaddr.sin_addr.s_addr = ::inet_addr(ip_addr.c_str());
    servaddr.sin_port = htons(port);

    if (::connect(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        std::cerr << "::connect failed" << std::endl;
        perror("connect failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    loop_ptr->addIoEvent(sockfd, readCallback, EPOLLIN, std::any());

    std::string data_str("zczjx--> aio udp client start");

    std::vector<uint8_t> data_buffer(data_str.begin(), data_str.end());

    size_t nw = ::send(sockfd, data_buffer.data(), data_buffer.size(), 0);
    if (nw < 0)
    {
        std::cout << "::send failed" << std::endl;
    }

    loop_ptr->processEvents();

    return 0;
}