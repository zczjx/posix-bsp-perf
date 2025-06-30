#include "objDetector.hpp"
#include <common/msg/ObjDetectMsg.hpp>
#include <common/msg/CameraSensorMsg.hpp>

#include <thread>
#include <chrono>

namespace apps
{
namespace data_recorder
{

ObjDetector::ObjDetector(ArgParser&& args, const json& nodes_ipc)
{
    for (const auto& node: nodes_ipc["object_detector"])
    {
        std::string name{node["name"]};
        std::string status{node["status"]};

        const json& subscriber = node["subscriber"];
        const json& publisher = node["publisher"];

        m_input_shmem_port = std::make_shared<SharedMemSubscriber>(subscriber["topic"],
                                                                   subscriber["shmem"],
                                                                   subscriber["shmem_slots"],
                                                                   subscriber["shmem_single_buffer_size"]);

        m_output_shmem_port = std::make_shared<SharedMemPublisher>(publisher["topic"],
                                                                   publisher["shmem"],
                                                                   publisher["shmem_slots"],
                                                                   publisher["shmem_single_buffer_size"]);
    }
}

void ObjDetector::runLoop()
{
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

} // namespace data_recorder
} // namespace apps