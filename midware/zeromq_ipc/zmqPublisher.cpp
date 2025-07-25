#include "zmqPublisher.hpp"

namespace midware
{
namespace zeromq_ipc
{
ZmqPublisher::ZmqPublisher(const std::string& topic): m_topic(topic)
{
    m_context = std::make_shared<zmq::context_t>(1);
    m_socket = std::make_shared<zmq::socket_t>(*m_context, ZMQ_PUB);
    m_socket->bind(m_topic);
}

ZmqPublisher::~ZmqPublisher()
{
    m_socket->close();
    m_context->close();
}

int ZmqPublisher::sendData(const void* data, size_t size)
{
    return sendData(data, size, false);
}

size_t ZmqPublisher::sendDataMore(const void* data, size_t size)
{
    return sendData(data, size, true);
}

size_t ZmqPublisher::sendDataLast(const void* data, size_t size)
{
    return sendData(data, size, false);
}

size_t ZmqPublisher::sendData(const void* data, size_t size, bool more)
{
    zmq::message_t message(data, size);
    auto result = m_socket->send(message, more ? zmq::send_flags::sndmore : zmq::send_flags::none);
    return result.value_or(0);
}

int ZmqPublisher::waitSync(const std::string& sync_topic)
{
    m_sync_topic = sync_topic;
    zmq::socket_t sync_socket(*m_context, zmq::socket_type::rep);
    sync_socket.bind(m_sync_topic);
    zmq::message_t sync_request;
    sync_socket.recv(sync_request, zmq::recv_flags::none);
    sync_socket.send(zmq::str_buffer(""), zmq::send_flags::none);
    sync_socket.close();
    return 0;
}

} // namespace zeromq_ipc
} // namespace midware