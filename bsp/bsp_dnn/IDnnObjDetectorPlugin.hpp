#ifndef __IDNN_OBJDETECTOR_PLUGIN_HPP__
#define __IDNN_OBJDETECTOR_PLUGIN_HPP__

#include "IDnnEngine.hpp"
#include <memory>
#include <string>
#include <vector>
#include <any>
#include <rockchip/rknn_api.h>

namespace bsp_dnn
{

struct ObjDetectInput
{
    /**
     * @brief Handle type for the object detector plugin.
     * default is opencv4
     * Possible values:
     * - "opencv4"
     * - "DecodeOutFrame"
     */
    std::string handleType{"opencv4"};
    /// any should be a pointer to image type
    std::any imageHandle;
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
    size_t model_input_channel;

    std::vector<rknn_tensor_attr> output_attrs{};
    rknn_input_output_num io_num;

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
    virtual int preProcess(ObjDetectParams& params, ObjDetectInput& inputData, IDnnEngine::dnnInput& outputData) = 0; // 纯虚函数
    virtual int postProcess(const std::string& labelTextPath, const ObjDetectParams& params,
                    std::vector<IDnnEngine::dnnOutput>& inputData, std::vector<ObjDetectOutputBox>& outputData) = 0; // 纯虚函数

    IDnnObjDetectorPlugin() = default;
    virtual ~IDnnObjDetectorPlugin() = default;
};

}

#define CREATE_PLUGIN_INSTANCE_YOLOV5(PLUGIN_CLASS) \
    extern "C" bsp_dnn::IDnnObjDetectorPlugin* create_yolov5() { \
        return new PLUGIN_CLASS(); \
    } \
    extern "C" void destroy_yolov5(bsp_dnn::IDnnObjDetectorPlugin* plugin) { \
        delete plugin; \
    }



#define CREATE_PLUGIN_INSTANCE_YOLOV8(PLUGIN_CLASS) \
    extern "C" bsp_dnn::IDnnObjDetectorPlugin* create_yolov8() { \
        return new PLUGIN_CLASS(); \
    } \
    extern "C" void destroy_yolov8(bsp_dnn::IDnnObjDetectorPlugin* plugin) { \
        delete plugin; \
    }

#endif // __IDNN_OBJDETECTOR_PLUGIN_HPP__