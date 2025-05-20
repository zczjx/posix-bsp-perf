#include "SensorClient.hpp"

namespace apps
{
namespace data_recorder
{

SensorClient::SensorClient(const json& sensor_context)
{
    m_type = sensor_context["type"];
    m_name = sensor_context["name"];
    m_zmq_transport = sensor_context["zmq_transport"];

    m_input_sub = std::make_shared<ZmqSubscriber>(m_zmq_transport);
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
} // namespace data_recorder
} // namespace apps