#ifndef __OBJDETECTAPP_HPP__
#define __OBJDETECTAPP_HPP__

#include <framework/BasePerfCase.hpp>
#include <shared/BspLogger.hpp>
#include <shared/ArgParser.hpp>
#include <bsp_dnn/dnnObjDetector.hpp>
#include <memory>
#include <string>
#include <iostream>
#include <vector>
#include <opencv2/opencv.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace bsp_perf {
namespace perf_cases {
using namespace bsp_dnn;

class ObjDetectApp : public bsp_perf::common::BasePerfCase
{
public:

    ObjDetectApp(bsp_perf::shared::ArgParser&& args):
        BasePerfCase(std::move(args)),
        m_logger{std::make_unique<bsp_perf::shared::BspLogger>("ObjDetectApp")}
    {
        m_logger->setPattern();
        auto& params = getArgs();
        std::string dnnType;
        std::string pluginPath;
        std::string labelTextPath;
        params.getOptionVal("--dnnType", dnnType);
        params.getOptionVal("--pluginPath", pluginPath);
        params.getOptionVal("--labelTextPath", labelTextPath);
        m_dnnObjDetector = std::make_unique<bsp_dnn::dnnObjDetector>(dnnType, pluginPath, labelTextPath);
    }
    ObjDetectApp(const ObjDetectApp&) = delete;
    ObjDetectApp& operator=(const ObjDetectApp&) = delete;
    ObjDetectApp(ObjDetectApp&&) = delete;
    ObjDetectApp& operator=(ObjDetectApp&&) = delete;
    ~ObjDetectApp()
    {
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Debug, "{} ObjDetectApp::~ObjDetectApp()", LOG_TAG);
        m_logger->printFileLog(bsp_perf::shared::BspLogger::LogLevel::Debug, "{} ObjDetectApp::~ObjDetectApp()", LOG_TAG);
        m_logger->printAsyncFileLog(bsp_perf::shared::BspLogger::LogLevel::Debug, "{} ObjDetectApp::~ObjDetectApp()", LOG_TAG);
    }
private:

    void onInit() override
    {
        auto& params = getArgs();
        std::string modelPath;
        params.getOptionVal("--modelPath", modelPath);
        m_dnnObjDetector->loadModel(modelPath);
        std::string imagePath;
        params.getOptionVal("--imagePath", imagePath);
        m_orig_image_ptr = std::make_shared<cv::Mat>(cv::imread(imagePath, cv::IMREAD_COLOR));
    }

    void onProcess() override
    {
        bsp_dnn::ObjDetectInput objDetectInput = {
            .handleType = "opencv4",
            .imageHandle = m_orig_image_ptr,
        };
        m_dnnObjDetector->pushInputData(std::make_shared<bsp_dnn::ObjDetectInput>(objDetectInput));
        setObjDetectParams(m_objDetectParams);
        m_dnnObjDetector->runObjDetect(m_objDetectParams);
        auto& objDetectOutput = m_dnnObjDetector->popOutputData();

        for (const auto& item : objDetectOutput)
            {
                m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Debug,
                    "Detected object: label={}, score={}, x={}, y={}, width={}, height={}",
                    item.label, item.score, item.bbox.left, item.bbox.right, item.bbox.top, item.bbox.bottom);
            }
        std::vector<cv::Scalar> colors = {
                cv::Scalar(255, 0, 0),    // Blue
                cv::Scalar(0, 255, 0),    // Green
                cv::Scalar(0, 0, 255),    // Red
                cv::Scalar(255, 255, 0),  // Cyan
                cv::Scalar(255, 0, 255),  // Magenta
                cv::Scalar(0, 255, 255),  // Yellow
                cv::Scalar(128, 0, 0),    // Maroon
                cv::Scalar(0, 128, 0),    // Olive
                cv::Scalar(0, 0, 128),    // Navy
                cv::Scalar(128, 128, 0),  // Teal
                cv::Scalar(128, 0, 128),  // Purple
                cv::Scalar(0, 128, 128)   // Aqua
        };
        std::map<std::string, cv::Scalar> labelColorMap;
        for (size_t i = 0; i < objDetectOutput.size(); ++i)
        {
            const auto& obj = objDetectOutput[i];
            if (labelColorMap.find(obj.label) == labelColorMap.end())
            {
                labelColorMap[obj.label] = colors[i % colors.size()];
            }
        }

        for (const auto& obj : objDetectOutput)
        {
            m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, "{} ObjDetectApp::onProcess() objDetectOutput: bbox: [{}, {}, {}, {}], score: {}, label: {}",
                LOG_TAG, obj.bbox.left, obj.bbox.top, obj.bbox.right, obj.bbox.bottom, obj.score, obj.label);
            cv::rectangle(*m_orig_image_ptr, cv::Point(obj.bbox.left, obj.bbox.top), cv::Point(obj.bbox.right, obj.bbox.bottom),
                    labelColorMap[obj.label], 2);
            cv::putText(*m_orig_image_ptr, obj.label, cv::Point(obj.bbox.left, obj.bbox.top + 12), cv::FONT_HERSHEY_COMPLEX, 0.4, cv::Scalar(256, 255, 255));
        }
    }

    void onRender() override
    {
        cv::imwrite("output.jpg", *m_orig_image_ptr);
    }

    void onRelease() override
    {
        m_dnnObjDetector.reset();
    }

private:
    void setObjDetectParams(ObjDetectParams& objDetectParams)
    {
        IDnnEngine::dnnInputShape shape;
        m_dnnObjDetector->getInputShape(shape);
        objDetectParams.model_input_width = shape.width;
        objDetectParams.model_input_height = shape.height;
        objDetectParams.model_input_channel = shape.channel;
        objDetectParams.conf_threshold = 0.25;
        objDetectParams.nms_threshold = 0.45;
        objDetectParams.scale_width = static_cast<float>(shape.width) / static_cast<float>(m_orig_image_ptr->cols);
        objDetectParams.scale_height = static_cast<float>(shape.height) / static_cast<float>(m_orig_image_ptr->rows);
        objDetectParams.pads.left = 0;
        objDetectParams.pads.right = 0;
        objDetectParams.pads.top = 0;
        objDetectParams.pads.bottom = 0;
        m_dnnObjDetector->getOutputQuantParams(objDetectParams.quantize_zero_points, objDetectParams.quantize_scales);
    }

private:
    std::string m_name {"[ObjDetectApp]:"};
    std::unique_ptr<bsp_perf::shared::BspLogger> m_logger{nullptr};
    std::unique_ptr<bsp_dnn::dnnObjDetector> m_dnnObjDetector{nullptr};
    bsp_dnn::ObjDetectParams m_objDetectParams{};
    std::shared_ptr<cv::Mat> m_orig_image_ptr{nullptr};
};

} // namespace perf_cases
} // namespace bsp_perf

#endif // __OBJDETECTAPP_HPP__