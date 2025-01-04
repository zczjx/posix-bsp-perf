#include <bsp_dnn/IDnnEngine.hpp>
#include "rknn/rknn.hpp"

namespace bsp_dnn
{

std::unique_ptr<IDnnEngine> IDnnEngine::create(const std::string& dnnType)
{
    if (dnnType.compare("trt") == 0)
    {
        // return std::make_unique<TensorRTDnn>();
        throw std::invalid_argument("TensorRT DNN is not implemented.");
    }
    else if (dnnType.compare("rknn") == 0)
    {
        return std::make_unique<rknn>();
    }
    else
    {
        throw std::invalid_argument("Invalid DNN type specified.");
    }
}

} // namespace bsp_dnn