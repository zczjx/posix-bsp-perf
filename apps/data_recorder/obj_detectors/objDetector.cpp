#include "objDetector.hpp"
#include <thread>
#include <chrono>

namespace apps
{
namespace data_recorder
{

ObjDetector::ObjDetector(const json& nodes_ipc, const std::string& g2dPlatform):
    m_g2dPlatform(g2dPlatform)
{
    for (const auto& node: nodes_ipc["object_detector"])
    {
        std::string name{node["name"]};
        std::string status{node["status"]};

        if (status.compare("enabled") == 0 && name.compare("yolov5_detector") == 0)
        {
            m_image_frame_adapter = std::make_shared<ImageFrameAdapter>(node, g2dPlatform);
        }
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