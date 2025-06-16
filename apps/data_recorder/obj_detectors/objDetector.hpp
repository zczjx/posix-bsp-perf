#ifndef __OBJ_DETECTOR_HPP__
#define __OBJ_DETECTOR_HPP__

#include <string>
#include <nlohmann/json.hpp>
#include <common/ImageFrameAdapter.hpp>

using json = nlohmann::json;

namespace apps
{
namespace data_recorder
{

class ObjDetector
{
public:
    ObjDetector(const json& nodes_ipc, const std::string& g2dPlatform);

    void runLoop();

private:
    std::string m_g2dPlatform;
    std::shared_ptr<ImageFrameAdapter> m_image_frame_adapter;
};

} // namespace data_recorder
} // namespace apps

#endif