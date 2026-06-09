#ifndef __SVS_TYPES_HPP__
#define __SVS_TYPES_HPP__

#include <array>
#include <cstddef>
#include <cstdint>
#include <bsp_image/ImageTypes.hpp>
#include <string>
#include <vector>

namespace bsp_perf
{
namespace svs
{

constexpr size_t kCameraCount = 4;

struct Matrix
{
    uint32_t rows{0};
    uint32_t cols{0};
    std::vector<double> values{};

    bool empty() const { return rows == 0 || cols == 0 || values.empty(); }
};

struct CameraParameters
{
    std::string name;
    Matrix distCoeffs;
    Matrix cameraMatrix;
    Matrix projectMatrix;
    Matrix transMatrix;
    bsp_perf::bsp_image::ImageSize size;
    Matrix scaleXY;
    Matrix shiftXY;
};

struct CameraConfig
{
    std::string name;
    std::string calibPath;
    std::string flipMode{"n"}; // "n", "r+", "r-", "m"
};

struct ProjectionLayout
{
    int shiftWidth{300};
    int shiftHeight{300};
    int calibrationMapWidth{600};
    int calibrationMapHeight{1000};
    int innerShiftWidth{20};
    int innerShiftHeight{50};
    int vehicleOffsetWidth{180};
    int vehicleOffsetHeight{200};

    int totalWidth() const { return calibrationMapWidth + 2 * shiftWidth; }
    int totalHeight() const { return calibrationMapHeight + 2 * shiftHeight; }
    int vehicleLeft() const { return shiftWidth + vehicleOffsetWidth + innerShiftWidth; }
    int vehicleRight() const { return totalWidth() - vehicleLeft(); }
    int vehicleTop() const { return shiftHeight + vehicleOffsetHeight + innerShiftHeight; }
    int vehicleBottom() const { return totalHeight() - vehicleTop(); }
    bsp_perf::bsp_image::ImageSize outputSize() const
    {
        return {
            static_cast<uint32_t>(totalWidth()),
            static_cast<uint32_t>(totalHeight()),
        };
    }
};

struct SurroundViewConfig
{
    std::string dataRoot;
    std::string weightImagePath;
    std::string carImagePath;
    ProjectionLayout layout;
    std::array<CameraConfig, kCameraCount> cameras{{
        {"front", "", "n"},
        {"left", "", "r-"},
        {"back", "", "m"},
        {"right", "", "r+"},
    }};
    bool enableAwb{true};
    bool enableLuminanceBalance{true};
};

struct FrameSet
{
    std::array<bsp_perf::bsp_image::ImageView, kCameraCount> cameras;
};

struct OutputFrame
{
    bsp_perf::bsp_image::ImageView image;
};

} // namespace svs
} // namespace bsp_perf

#endif // __SVS_TYPES_HPP__
