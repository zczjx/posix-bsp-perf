#ifndef __ZMQ_SUBSCRIBER_HPP__
#define __ZMQ_SUBSCRIBER_HPP__

#include <memory>
#include <vector>
#include <zmq.hpp>

namespace midware
{
namespace zeromq_ipc
{
class ZmqSubscriber
{
public:
    ZmqSubscriber(const ZmqSubscriber&) = delete;
    ZmqSubscriber& operator=(const ZmqSubscriber&) = delete;
    ZmqSubscriber(ZmqSubscriber&&) = delete;
    ZmqSubscriber& operator=(ZmqSubscriber&&) = delete;
    /**
     * @brief Construct a new Zmq Subscriber object
     * 
     * @param topic example: "tcp://127.0.0.1:5555"
     */
    explicit ZmqSubscriber(const std::string& topic);

    /**
     * @brief Destroy the Zmq Subscriber object
     */
    virtual ~ZmqSubscriber();

    /**
     * @brief Receive data from the topic
     *
     * @param data
     * @return int
     */
    int receiveData(std::vector<uint8_t>& data);

private:
    std::shared_ptr<zmq::context_t> m_context;
    std::shared_ptr<zmq::socket_t> m_socket;

};
} // namespace zeromq_ipc
} // namespace midware


#endif
