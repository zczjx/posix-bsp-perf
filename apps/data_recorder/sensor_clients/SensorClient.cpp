#include "SensorClient.hpp"

namespace apps
{
namespace data_recorder
{

SensorClient::SensorClient(const json& sensor_context)
{
    m_type = sensor_context["type"];
    m_name = sensor_context["name"];
    m_ipc_topic = sensor_context["ipc_topic"];

    m_input_sub = std::make_shared<ZmqSubscriber>(m_ipc_topic);
}

SensorClient::~SensorClient()
{
    m_input_sub.reset();
}

size_t SensorClient::recvIpcData(uint8_t* buffer, size_t bytes)
{
    return m_input_sub->receiveData(buffer, bytes);
}

int SensorClient::receiveTpcData(std::vector<uint8_t>& data)
{
    return m_input_sub->receiveData(data);
}
} // namespace data_recorder
} // namespace apps