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

    size_t sendData(std::shared_ptr<uint8_t[]> buffer, size_t size)
    {
        return sendData(buffer.get(), size);
    }

    /**
     * @brief Send data to zmq with send more flag, it means that the data is not the last part of the message
     *
     * @param data
     * @param size
     * @return size_t the number of bytes sent
     */
    size_t sendDataMore(const void* data, size_t size);

    size_t sendDataMore(std::shared_ptr<uint8_t[]> buffer, size_t size)
    {
        return sendDataMore(buffer.get(), size);
    }

    /**
     * @brief Send data to zmq with send last flag, it means that the data is the last part of the message
     *
     * @param data
     * @param size
     * @return size_t the number of bytes sent
     */
    size_t sendDataLast(const void* data, size_t size);

    size_t sendDataLast(std::shared_ptr<uint8_t[]> buffer, size_t size)
    {
        return sendDataLast(buffer.get(), size);
    }


    size_t sendData(const void* data, size_t size, bool more);

    size_t sendData(std::shared_ptr<uint8_t[]> buffer, size_t size, bool more)
    {
        return sendData(buffer.get(), size, more);
    }

    const std::string& getTopic() const
    {
        return m_topic;
    }

private:
    std::shared_ptr<zmq::context_t> m_context;
    std::shared_ptr<zmq::socket_t> m_socket;
    std::string m_topic;
};

} // namespace zeromq_ipc
} // namespace midware


#endif
