#ifndef _CAMERA_SENSOR_MSG_HPP_
#define _CAMERA_SENSOR_MSG_HPP_

#include <msgpack.hpp>
#include <string>
#include <cstdint>

namespace apps
{
namespace data_recorder
{
struct CameraSensorMsg
{
    std::string publisher_id{};
    std::string pixel_format{};
    int width{0};
    int height{0};
    size_t data_size{0}; // in bytes
    size_t slot_index{0};
    MSGPACK_DEFINE(publisher_id, pixel_format, width, height, data_size, slot_index);
};

} // namespace data_recorder
} // namespace apps
#endif