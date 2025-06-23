#include "SensorClient.hpp"

namespace apps
{
namespace data_recorder
{

SensorClient::SensorClient(const json& sensor_context)
{
    m_type = sensor_context["type"];
    m_name = sensor_context["name"];
}

} // namespace data_recorder
} // namespace apps