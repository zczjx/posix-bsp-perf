
#ifndef __SENSOR_CLIENT_HPP__
#define __SENSOR_CLIENT_HPP__

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <memory>
#include <zeromq_ipc/zmqSubscriber.hpp>

using json = nlohmann::json;
using namespace midware::zeromq_ipc;

namespace apps
{
namespace data_recorder
{

class SensorClient
{
public:
    explicit SensorClient(const json& sensor_context);
    virtual ~SensorClient();

    virtual void runLoop() = 0;

protected:
    size_t recvIpcData(uint8_t* buffer, size_t bytes);

    int receiveTpcData(std::vector<uint8_t>& data);

    std::shared_ptr<ZmqSubscriber> getInputSub() const
    {
        return m_input_sub;
    }

private:
    std::shared_ptr<ZmqSubscriber> m_input_sub;
    std::string m_type;
    std::string m_name;
    std::string m_ipc_topic;
};


} // namespace data_recorder
} // namespace apps

#endif // __SENSOR_CLIENT_HPP__