#ifndef __SVS_TYPES_HPP__
#define __SVS_TYPES_HPP__

#include <array>
#include <bsp_image/ImageBuffer.hpp>
#include <cstddef>
#include <cstdint>
#include <string>

namespace bsp_perf
{
namespace svs
{

constexpr size_t kCameraCount = 4;

struct CameraPose
{
    double x{0.0};
    double y{0.0};
    double z{0.0};
    double roll{0.0};
    double pitch{0.0};
    double yaw{0.0};
};

struct CameraConfig
{
    std::string name;
    CameraPose pose;
    double fov{90.0};
};

struct GroundExtent
{
    double front{9.0};
    double rear{9.0};
    double left{16.0};
    double right{16.0};
};

struct VehicleSize
{
    double length{4.7};
    double width{1.9};
};

struct ProjectionRegion
{
    double nearDistance{3.0};
    double nearHalfWidth{1.5};
    double farDistance{12.0};
    double farHalfWidth{6.0};
};

struct SurroundViewConfig
{
    uint32_t outputWidth{640};
    uint32_t outputHeight{360};
    std::string outputPixelFormat{"RGB888"};
    double groundZ{0.0};
    GroundExtent groundExtent;
    VehicleSize vehicleSize;
    ProjectionRegion projectionRegion;
    std::array<CameraConfig, kCameraCount> cameras;
    double featherPower{0.5};
    bool fillUncovered{true};
    double inpaintRadius{5.0};
    std::string vehicleImagePath;
};

struct FrameSet
{
    std::array<bsp_perf::bsp_image::ImageView, kCameraCount> cameras;
};

struct OutputFrame
{
    bsp_perf::bsp_image::ImageBuffer image;
};

} // namespace svs
} // namespace bsp_perf

#endif // __SVS_TYPES_HPP__
