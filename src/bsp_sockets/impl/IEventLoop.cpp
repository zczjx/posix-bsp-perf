#include <bsp_sockets/IEventLoop.hpp>
#include "EventLoopEpoll.hpp"
#include "EventLoopPoll.hpp"
#include "EventLoopLibevent.hpp"
#include "BspSocketException.hpp"


namespace bsp_sockets{

std::shared_ptr<IEventLoop> IEventLoop::create(const std::string flag) {
    if (flag.compare("epoll") == 0)
    {
        return std::make_shared<EventLoopEpoll>();
    }
    else if (flag.compare("poll") == 0)
    {
        return std::make_shared<EventLoopPoll>();
    }
    else if (flag.compare("libevent")==0)
    {
        return std::make_shared<EventLoopLibevent>();
    }
    else
    {
        throw BspSocketException("Unknown EventLoop type");
    }
}
}
