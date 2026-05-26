#ifndef __SVS_TYPES_HPP__
#define __SVS_TYPES_HPP__

#include <array>
#include <cstdint>
#include <map>
#include <opencv2/core.hpp>
#include <string>
#include <vector>

namespace bsp_perf
{
namespace svs
{

constexpr size_t kCameraCount = 4;

struct CameraParameters
{
    std::string name;
    cv::Mat distCoeffs;
    cv::Mat cameraMatrix;
    cv::Mat projectMatrix;
    cv::Mat transMatrix;
    cv::Size size;
    cv::Mat scaleXY;
    cv::Mat shiftXY;
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
    cv::Size outputSize() const { return cv::Size(totalWidth(), totalHeight()); }
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
    std::array<cv::Mat, kCameraCount> cameras;
};

struct OutputFrame
{
    cv::Mat image;
};

} // namespace svs
} // namespace bsp_perf

#endif // __SVS_TYPES_HPP__
