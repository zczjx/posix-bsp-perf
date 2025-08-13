#ifndef _OBJ_DETECT_MSG_HPP_
#define _OBJ_DETECT_MSG_HPP_

#include <msgpack.hpp>
#include <string>
#include <cstdint>
#include <array>
#include <bsp_dnn/IDnnObjDetectorPlugin.hpp>
#include "CameraSensorMsg.hpp"

namespace apps
{
namespace data_recorder
{
struct ObjDetectMsg
{
    static constexpr uint32_t MAX_BOXES = 64;
    std::string publisher_id{};
    CameraSensorMsg original_frame{};
    std::array<bsp_dnn::ObjDetectOutputBox, MAX_BOXES> output_boxes{};
    uint32_t valid_box_count{0};
    MSGPACK_DEFINE(publisher_id, original_frame, output_boxes, valid_box_count);
};

} // namespace data_recorder
} // namespace apps
#endif