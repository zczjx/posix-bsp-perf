#ifndef __ZMQ_PUBLISHER_HPP__
#define __ZMQ_PUBLISHER_HPP__

#include <memory>
#include <zmq.hpp>

namespace midware
{
namespace zeromq_ipc
{
class ZmqPublisher
{
public:
    ZmqPublisher();
    ~ZmqPublisher();

private:
    std::shared_ptr<zmq::context_t> m_context;
    std::shared_ptr<zmq::socket_t> m_socket;
};

} // namespace zeromq_ipc
} // namespace midware


#endif
