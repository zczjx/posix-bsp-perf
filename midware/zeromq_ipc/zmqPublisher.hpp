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
    ZmqPublisher(const ZmqPublisher&) = delete;
    ZmqPublisher& operator=(const ZmqPublisher&) = delete;
    ZmqPublisher(ZmqPublisher&&) = delete;
    ZmqPublisher& operator=(ZmqPublisher&&) = delete;
    /**
     * @brief Construct a new Zmq Publisher object
     *
     * @param topic example: "tcp://127.0.0.1:5555"
     */
    explicit ZmqPublisher(const std::string& topic);

    virtual ~ZmqPublisher();

    int sendData(const void* data, size_t size);

private:
    std::shared_ptr<zmq::context_t> m_context;
    std::shared_ptr<zmq::socket_t> m_socket;
};

} // namespace zeromq_ipc
} // namespace midware


#endif
