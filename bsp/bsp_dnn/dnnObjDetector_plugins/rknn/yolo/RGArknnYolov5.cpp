#include "RGArknnYolov5.hpp"
#include <bsp_dnn/dnnObjDetector_plugins/common/G2dPreprocessHelper.hpp>
#include <memory>
#include <iostream>
#include <cstring>

namespace bsp_dnn
{
int RGArknnYolov5::preProcess(ObjDetectParams& params, ObjDetectInput& inputData, IDnnEngine::dnnInput& outputData)
{
    bsp_perf::bsp_image::ImageView inputImage = inputData.image;
    if (inputImage.empty())
    {
        throw std::invalid_argument("inputData.image is empty.");
    }

    if (m_g2d == nullptr)
    {
        throw std::invalid_argument("m_g2d is nullptr.");
    }

    bsp_perf::bsp_image::ImageView rgb888Image{};
    // 硬件加速：YUV → RGB + Resize
    // 注意：RGA 是零拷贝，直接操作 m_rknn_input_buf 内存
    int ret = g2dResizeToHost(*m_g2d, inputImage, m_rknn_input_buf,
                              static_cast<uint32_t>(params.model_input_width),
                              static_cast<uint32_t>(params.model_input_height),
                              "RGB888", rgb888Image);
    if (ret != 0)
    {
        throw std::runtime_error("imageResize failed with code: " + std::to_string(ret));
    }

    // 准备 DNN 输入数据
    size_t rknn_input_size = rgb888Image.desc.dataSize;
    outputData.index = 0;
    outputData.shape.width = params.model_input_width;
    outputData.shape.height = params.model_input_height;
    outputData.shape.channel = params.model_input_channel;
    outputData.size = rknn_input_size;
    if (outputData.buf.size() != outputData.size)
    {
        outputData.buf.resize(outputData.size);
    }
    outputData.dataType = "UINT8";

    // RGA 零拷贝：m_rknn_input_buf 已经包含转换后的数据
    std::memcpy(outputData.buf.data(), m_rknn_input_buf.data(), outputData.size);

    return 0;
}

int RGArknnYolov5::postProcess(const std::string& labelTextPath, const ObjDetectParams& params,
        std::vector<IDnnEngine::dnnOutput>& inputData, std::vector<ObjDetectOutputBox>& outputData)
{
    if (inputData.size() != RKNN_YOLOV5_OUTPUT_BATCH || labelTextPath.empty())
    {
        throw std::invalid_argument("The size of inputData is not equal to RKNN_YOLOV5_OUTPUT_BATCH or labelTextPath is empty.");
    }

    int ret = m_yoloPostProcess.initLabelMap(labelTextPath);
    if (ret != 0)
    {
        return ret;
    }
    return m_yoloPostProcess.runPostProcess(params, inputData, outputData);
}

}   // namespace bsp_dnn