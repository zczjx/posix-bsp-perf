#include "zmqPublisher.hpp"

namespace midware
{
namespace zeromq_ipc
{
ZmqPublisher::ZmqPublisher(const std::string& topic)
{
    m_context = std::make_shared<zmq::context_t>(1);
    m_socket = std::make_shared<zmq::socket_t>(*m_context, ZMQ_PUB);
    m_socket->bind(topic);
}

ZmqPublisher::~ZmqPublisher()
{
    m_socket->close();
    m_context->close();
}

int ZmqPublisher::sendData(const void* data, size_t size)
{
    zmq::message_t message(data, size);
    return m_socket->send(message);
}





} // namespace zeromq_ipc
} // namespace midware