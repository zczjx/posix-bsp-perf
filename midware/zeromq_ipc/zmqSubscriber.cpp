#include "zmqSubscriber.hpp"
#include <iostream>
#include <cstring>
#include <algorithm>

namespace midware
{
namespace zeromq_ipc
{
ZmqSubscriber::ZmqSubscriber(const std::string& topic)
    : m_topic(topic)
{
    m_context = std::make_shared<zmq::context_t>(1);
    m_socket = std::make_shared<zmq::socket_t>(*m_context, ZMQ_SUB);
    m_socket->connect(m_topic);
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

size_t ZmqSubscriber::receiveDataMore(uint8_t* buffer, size_t bytes)
{
    zmq::message_t message;
    m_socket->recv(&message);
    if (message.size() > bytes)
    {
        std::cerr << "receiveDataMore: message size is greater than the user buffer size" << std::endl;
        return 0;
    }
    std::memcpy(buffer, message.data(), message.size());
    size_t bytes_index = message.size();

    while(message.more())
    {
        m_socket->recv(&message);
        if (message.size() > (bytes - bytes_index))
        {
            std::cerr << "receiveDataMore: next message size is greater than the user buffer size" << std::endl;
            return 0;
        }
        std::memcpy(buffer + bytes_index, message.data(), message.size());
        bytes_index += message.size();
    }
    return bytes_index;
}

int ZmqSubscriber::receiveDataMore(std::vector<uint8_t>& data)
{
    zmq::message_t message;
    m_socket->recv(&message);
    data.assign(static_cast<uint8_t*>(message.data()), static_cast<uint8_t*>(message.data()) + message.size());

    while(message.more())
    {
        m_socket->recv(&message);
        data.insert(data.end(), static_cast<uint8_t*>(message.data()), static_cast<uint8_t*>(message.data()) + message.size());
    }

    return 0;
}

size_t ZmqSubscriber::receiveData(uint8_t* buffer, size_t bytes)
{
    zmq::message_t message;
    m_socket->recv(&message);
    size_t actual_bytes = std::min(message.size(), bytes);
    std::memcpy(buffer, message.data(), actual_bytes);
    return actual_bytes;
}

int ZmqSubscriber::replySync(const std::string& sync_topic)
{
    m_sync_topic = sync_topic;
    zmq::socket_t sync_socket(*m_context, zmq::socket_type::req);
    sync_socket.connect(m_sync_topic);
    sync_socket.send(zmq::message_t("READY"), zmq::send_flags::none);
    zmq::message_t sync_reply;
    sync_socket.recv(sync_reply, zmq::recv_flags::none);
    sync_socket.close();
    return 0;
}
} // namespace zeromq_ipc
} // namespace midware