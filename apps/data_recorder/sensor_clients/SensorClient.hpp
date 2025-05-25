
#ifndef __SENSOR_CLIENT_HPP__
#define __SENSOR_CLIENT_HPP__

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <memory>
#include <zeromq_ipc/zmqSubscriber.hpp>
#include <zeromq_ipc/zmqPublisher.hpp>
using json = nlohmann::json;
using namespace midware::zeromq_ipc;

namespace apps
{
namespace data_recorder
{

class SensorClient
{
public:
    explicit SensorClient(const json& sensor_context, const json& sensor_ipc);
    virtual ~SensorClient();

    virtual void runLoop() = 0;

protected:
    size_t recvIpcData(uint8_t* buffer, size_t bytes);

    size_t recvIpcData(std::shared_ptr<uint8_t[]> buffer, size_t bytes);

    size_t recvIpcDataMore(std::shared_ptr<uint8_t[]> buffer, size_t bytes);

    int recvIpcData(std::vector<uint8_t>& data);

    int recvIpcDataMore(std::vector<uint8_t>& data);

    size_t pubSensorData(const uint8_t* data, size_t bytes);

    size_t pubSensorData(std::shared_ptr<uint8_t[]> buffer, size_t bytes);

    size_t pubSensorInfo(const uint8_t* data, size_t bytes);

    size_t pubSensorInfo(std::shared_ptr<uint8_t[]> buffer, size_t bytes);

    std::shared_ptr<ZmqSubscriber> getInputSub() const
    {
        return m_input_sub;
    }

    std::shared_ptr<ZmqPublisher> getZmqPubData() const
    {
        return m_zmq_pub_data;
    }

    std::shared_ptr<ZmqPublisher> getZmqPubInfo() const
    {
        return m_zmq_pub_info;
    }

private:
    std::shared_ptr<ZmqSubscriber> m_input_sub;
    std::string m_type;
    std::string m_name;
    std::string m_zmq_transport;

    std::shared_ptr<ZmqPublisher> m_zmq_pub_data;
    std::shared_ptr<ZmqPublisher> m_zmq_pub_info;

};


} // namespace data_recorder
} // namespace apps

#endif // __SENSOR_CLIENT_HPP__