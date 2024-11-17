#include "rknnYolov5.hpp"
#include <opencv2/opencv.hpp>
#include <memory>
#include <any>
#include <iostream>

namespace bsp_dnn
{

int rknnYolov5::preProcess(const ObjDetectParams& params, ObjDetectInput& inputData, IDnnEngine::dnnInput& outputData)
{
    if (inputData.handleType.compare("opencv4") != 0)
    {
        throw std::invalid_argument("Only opencv4 is supported.");
    }

    auto orig_image_ptr = std::any_cast<std::shared_ptr<cv::Mat>>(inputData.imageHandle);
    if (orig_image_ptr == nullptr)
    {
        throw std::invalid_argument("inputData.imageHandle is nullptr.");
    }
    cv::Mat orig_image = *orig_image_ptr;
    cv::Mat rgb_image;
    cv::cvtColor(orig_image, rgb_image, cv::COLOR_BGR2RGB);
    auto img_width  = rgb_image.cols;
    auto img_height = rgb_image.rows;

    if ((img_width != params.model_input_width) || (img_height != params.model_input_height))
    {
        cv::resize(rgb_image, rgb_image, cv::Size(params.model_input_width, params.model_input_height), 0, 0, cv::INTER_LINEAR);
    }

    outputData.index = 0;
    outputData.shape.width = params.model_input_width;
    outputData.shape.height = params.model_input_height;
    outputData.shape.channel = 3;
    outputData.size = rgb_image.total() * rgb_image.elemSize();
    outputData.buf.reset(new uint8_t[outputData.size]);
    std::memcpy(outputData.buf.get(), rgb_image.data, outputData.size);
    return 0;
}

int rknnYolov5::postProcess(const std::string& labelTextPath, const ObjDetectParams& params,
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