#include "rknnYolov5.hpp"
#include <opencv2/opencv.hpp>
#include <memory>

namespace bsp_dnn
{

int rknnYolov5::preProcess(ObjDetectInput& inputData, IDnnEngine::dnnInput& outputData)
{
    if (!inputData.handleType.compare("opencv4"))
    {
        throw std::invalid_argument("Only opencv4 is supported.");
    }

    if (nullptr == inputData.imageHandle)
    {
        throw std::invalid_argument("imageHandle is nullptr.");
    }

    std::shared_ptr<cv::Mat> imagePtr(static_cast<cv::Mat*>(inputData.imageHandle));
    cv::Mat rgb_image;
    cv::cvtColor(*imagePtr, rgb_image, cv::COLOR_BGR2RGB);
    auto img_width  = rgb_image.cols;
    auto img_height = rgb_image.rows;

    if ((img_width != outputData.shape.width) || (img_height != outputData.shape.height))
    {
        cv::resize(rgb_image, rgb_image, cv::Size(outputData.shape.width, outputData.shape.width), 0, 0, cv::INTER_LINEAR);
    }
    outputData.buf = rgb_image.data;
    outputData.size = rgb_image.total() * rgb_image.elemSize();
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