#include "zmqSubscriber.hpp"

namespace midware
{
namespace zeromq_ipc
{
ZmqSubscriber::ZmqSubscriber(const std::string& topic)
{
    m_context = std::make_shared<zmq::context_t>(1);
    m_socket = std::make_shared<zmq::socket_t>(*m_context, ZMQ_SUB);
    m_socket->connect(topic);
    m_socket->set(zmq::sockopt::subscribe, "");
}

ZmqSubscriber::~ZmqSubscriber()
{
    m_socket->close();
    m_context->close();
}

int ZmqSubscriber::receiveData(std::vector<uint8_t>& data)
{
    zmq::message_t message;
    m_socket->recv(&message);
    data.assign(static_cast<uint8_t*>(message.data()), static_cast<uint8_t*>(message.data()) + message.size());
    return 0;
}
} // namespace zeromq_ipc
} // namespace midware