#ifndef __IDNN_OBJDETECTOR_PLUGIN_HPP__
#define __IDNN_OBJDETECTOR_PLUGIN_HPP__

#include "IDnnEngine.hpp"
#include <memory>
#include <string>
#include <vector>

namespace bsp_dnn
{

struct ObjDetectInput
{
    /// default is opencv4, will add more types later
    std::string handleType{"opencv4"};
    void* imageHandle{nullptr};
};

template <typename T>
struct bboxRect
{
    T left;
    T right;
    T top;
    T bottom;
};

struct ObjDetectOutputBox
{
    bboxRect<int> bbox{};
    float score{0.0};
    std::string label{};
};

struct ObjDetectParams
{
    size_t model_input_width;
    size_t model_input_height;
    float conf_threshold;
    float nms_threshold;
    float scale_width;
    float scale_height;
    bboxRect<int> pads;
    // quantization params
    std::vector<int32_t> quantize_zero_points;
    std::vector<float> quantize_scales;
};



class IDnnObjDetectorPlugin
{
public:
    virtual int preProcess(ObjDetectInput& inputData, IDnnEngine::dnnInput& outputData) = 0; // 纯虚函数
    virtual int postProcess(const std::string& labelTextPath, const ObjDetectParams& params,
                    std::vector<IDnnEngine::dnnOutput>& inputData, std::vector<ObjDetectOutputBox>& outputData) = 0; // 纯虚函数
    virtual ~IDnnObjDetectorPlugin() = default;
};

}

#define CREATE_PLUGIN_INSTANCE(PLUGIN_CLASS) \
    extern "C" bsp_dnn::IDnnObjDetectorPlugin* create() { \
        return new PLUGIN_CLASS(); \
    } \
    extern "C" void destroy(bsp_dnn::IDnnObjDetectorPlugin* plugin) { \
        delete plugin; \
    }

#endif // __IDNN_OBJDETECTOR_PLUGIN_HPP__