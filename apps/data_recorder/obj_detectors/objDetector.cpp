#include "objDetector.hpp"
#include <thread>
#include <chrono>

namespace apps
{
namespace data_recorder
{

ObjDetector::ObjDetector(const json& nodes_ipc)
{
    for (const auto& node: nodes_ipc["object_detector"])
    {
        std::string name{node["name"]};
        std::string status{node["status"]};
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