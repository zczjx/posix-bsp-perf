#ifndef __OBJ_DETECTOR_HPP__
#define __OBJ_DETECTOR_HPP__

#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace apps
{
namespace data_recorder
{

class ObjDetector
{
public:
    ObjDetector(const json& nodes_ipc);

    void runLoop();

private:
};

} // namespace data_recorder
} // namespace apps

#endif