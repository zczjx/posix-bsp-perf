#include "IDnn.hpp"
#include "rknn/rknn.hpp"

namespace bsp_dnn
{

template <typename T>
std::unique_ptr<IDnn<T>> IDnn<T>::create(const std::string& dnnType, const std::string& preProcessPluginPath, const std::string& postProcessPluginPath)
{
    if (dnnType.compare("trt"))
    {
        // return std::make_unique<TensorRTDnn>();
        throw std::invalid_argument("TensorRT DNN is not implemented.");
    }
    else if (dnnType.compare("rknn"))
    {
        return std::make_unique<rknn<T>>(preProcessPluginPath, postProcessPluginPath);
    }
    else
    {
        throw std::invalid_argument("Invalid DNN type specified.");
    }
}

} // namespace bsp_dnn