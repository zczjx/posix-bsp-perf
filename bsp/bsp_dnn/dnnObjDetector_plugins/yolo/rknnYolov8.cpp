#include "rknnYolov8.hpp"
#include <opencv2/opencv.hpp>
#include <memory>
#include <any>
#include <iostream>

namespace bsp_dnn
{

int rknnYolov8::preProcess(ObjDetectParams& params, ObjDetectInput& inputData, IDnnEngine::dnnInput& outputData)
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
    cv::Size target_size(params.model_input_width, params.model_input_height);
    cv::Mat padded_image(target_size.height, target_size.width, CV_8UC3);
    float min_scale = std::min(params.scale_width, params.scale_height);
    params.scale_width = min_scale;
    params.scale_height = min_scale;
    cv::Mat resized_image;
    cv::resize(rgb_image, resized_image, cv::Size(), min_scale, min_scale);

    // 计算填充大小
    int pad_width = target_size.width - resized_image.cols;
    int pad_height = target_size.height - resized_image.rows;
    params.pads.left = pad_width / 2;
    params.pads.right = pad_width - params.pads.left;
    params.pads.top = pad_height / 2;
    params.pads.bottom = pad_height - params.pads.top;

    // 在图像周围添加填充
    cv::copyMakeBorder(resized_image, padded_image, params.pads.top, params.pads.bottom, params.pads.left, params.pads.right, cv::BORDER_CONSTANT, cv::Scalar(128, 128, 128));

    outputData.index = 0;
    outputData.shape.width = params.model_input_width;
    outputData.shape.height = params.model_input_height;
    outputData.shape.channel = params.model_input_channel;
    outputData.size = padded_image.total() * padded_image.elemSize();
    if (outputData.buf.size() != outputData.size)
    {
        outputData.buf.resize(outputData.size);
    }
    outputData.dataType = "UINT8";
    std::memcpy(outputData.buf.data(), padded_image.data, outputData.size);
    return 0;
}

int rknnYolov8::postProcess(const std::string& labelTextPath, const ObjDetectParams& params,
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