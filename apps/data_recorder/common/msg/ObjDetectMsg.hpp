#ifndef _OBJ_DETECT_MSG_HPP_
#define _OBJ_DETECT_MSG_HPP_

#include <msgpack.hpp>
#include <string>
#include <cstdint>
#include <vector>
#include <bsp_dnn/IDnnObjDetectorPlugin.hpp>
#include "CameraSensorMsg.hpp"

namespace apps
{
namespace data_recorder
{
struct ObjDetectMsg
{
    std::string publisher_id{};
    CameraSensorMsg original_frame{};
    std::vector<bsp_dnn::ObjDetectOutputBox> output_boxes{};
    MSGPACK_DEFINE(publisher_id, original_frame, output_boxes);
};

} // namespace data_recorder
} // namespace apps
#endif