#include "SensorClient.hpp"

namespace apps
{
namespace data_recorder
{

SensorClient::SensorClient(const json& sensor_context, const json& sensor_ipc)
{
    m_type = sensor_context["type"];
    m_name = sensor_context["name"];
    m_zmq_transport = sensor_context["zmq_transport"];
    m_input_sub = std::make_shared<ZmqSubscriber>(m_zmq_transport);
    m_zmq_pub_data = std::make_shared<ZmqPublisher>(sensor_ipc["zmq_pub_data"]);
    m_zmq_pub_info = std::make_shared<ZmqPublisher>(sensor_ipc["zmq_pub_info"]);
}

SensorClient::~SensorClient()
{
    m_input_sub.reset();
}

size_t SensorClient::recvIpcData(uint8_t* buffer, size_t bytes)
{
    return m_input_sub->receiveData(buffer, bytes);
}


size_t SensorClient::recvIpcData(std::shared_ptr<uint8_t[]> buffer, size_t bytes)
{
    return m_input_sub->receiveData(buffer, bytes);
}

size_t SensorClient::recvIpcDataMore(std::shared_ptr<uint8_t[]> buffer, size_t bytes)
{
    return m_input_sub->receiveDataMore(buffer, bytes);
}

int SensorClient::recvIpcData(std::vector<uint8_t>& data)
{
    return m_input_sub->receiveData(data);
}

int SensorClient::recvIpcDataMore(std::vector<uint8_t>& data)
{
    return m_input_sub->receiveDataMore(data);
}

size_t SensorClient::pubSensorData(std::shared_ptr<uint8_t[]> buffer, size_t bytes)
{
    return m_zmq_pub_data->sendData(buffer, bytes);
}

size_t SensorClient::pubSensorInfo(std::shared_ptr<uint8_t[]> buffer, size_t bytes)
{
    return m_zmq_pub_info->sendData(buffer, bytes);
}

size_t SensorClient::pubSensorData(const uint8_t* data, size_t bytes)
{
    return m_zmq_pub_data->sendData(data, bytes);
}

size_t SensorClient::pubSensorInfo(const uint8_t* data, size_t bytes)
{
    return m_zmq_pub_info->sendData(data, bytes);
}

} // namespace data_recorder
} // namespace apps