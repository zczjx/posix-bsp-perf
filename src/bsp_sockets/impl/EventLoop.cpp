#include <bsp_sockets/EventLoop.hpp>
#include "bsp_sockets/EventLoop_Epoll.hpp"
#include "bsp_sockets/EventLoop_Poll.hpp"


namespace bsp_sockets{

std::shared_ptr<EventLoop> EventLoop::create(const int32_t flag) {
    if (flag == 0) {
        return std::make_shared<EventLoop_Epoll>();
    } else if (flag == 1) {
        return std::make_shared<EventLoop_Poll>();
    } else {
        throw std::invalid_argument("Unknown EventLoop type");
    }
}
}
