#include <bsp_sockets/EventLoop.hpp>
#include "bsp_sockets/EventLoopEpoll.hpp"
#include "bsp_sockets/EventLoopPoll.hpp"


namespace bsp_sockets{

std::shared_ptr<EventLoop> EventLoop::create(const std::string flag) {
    if (flag == "epoll") {
        return std::make_shared<EventLoopEpoll>();
    } else if (flag == "poll") {
        return std::make_shared<EventLoopPoll>();
    } else {
        throw std::invalid_argument("Unknown EventLoop type");
    }
}
}