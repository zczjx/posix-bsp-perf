#include <bsp_sockets/IEventLoop.hpp>
#include "EventLoopEpoll.hpp"
#include "EventLoopPoll.hpp"
#include "BspSocketException.hpp"


namespace bsp_sockets{

std::shared_ptr<IEventLoop> IEventLoop::create(const std::string flag) {
    if (flag.compare("epoll"))
    {
        return std::make_shared<EventLoopEpoll>();
    }
    else if (flag.compare("poll"))
    {
        return std::make_shared<EventLoopPoll>();
    }
    else
    {
        throw BspSocketException("Unknown EventLoop type");
    }
}
}