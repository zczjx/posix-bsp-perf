#include <bsp_dnn/IDnnEngine.hpp>

// Conditional include based on build platform
#ifdef BUILD_PLATFORM_RK35XX
#include "rknn/rknn.hpp"
#endif

#ifdef BUILD_PLATFORM_JETSON
#include "tensorrt/tensorrt.hpp"
#endif

namespace bsp_dnn
{

std::unique_ptr<IDnnEngine> IDnnEngine::create(const std::string& dnnType)
{
#ifdef BUILD_PLATFORM_JETSON
    if (dnnType.compare("trt") == 0)
    {
        return std::make_unique<tensorrt>();
    }
#endif

#ifdef BUILD_PLATFORM_RK35XX
    if (dnnType.compare("rknn") == 0)
    {
        return std::make_unique<rknn>();
    }
#endif

    // If no matching platform or invalid dnnType
    throw std::invalid_argument("Invalid DNN type '" + dnnType + "' for current platform.");
}

} // namespace bsp_dnn