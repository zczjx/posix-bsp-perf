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
        m_orig_image = cv::imread(imagePath, cv::IMREAD_COLOR);
    }

    void onProcess() override
    {
        bsp_dnn::ObjDetectInput objDetectInput;
        objDetectInput.handleType = "opencv4";
        objDetectInput.imageHandle = &m_orig_image;
        m_dnnObjDetector->pushInputData(std::make_shared<bsp_dnn::ObjDetectInput>(objDetectInput));
        setObjDetectParams(m_objDetectParams);
        m_dnnObjDetector->runObjDetect(m_objDetectParams);
        auto& objDetectOutput = m_dnnObjDetector->popOutputData();

        for (const auto& obj : objDetectOutput)
        {
            m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, "{} ObjDetectApp::onProcess() objDetectOutput: bbox: [{}, {}, {}, {}], score: {}, label: {}",
                LOG_TAG, obj.bbox.left, obj.bbox.top, obj.bbox.right, obj.bbox.bottom, obj.score, obj.label);
            cv::rectangle(m_orig_image, cv::Point(obj.bbox.left, obj.bbox.top), cv::Point(obj.bbox.right, obj.bbox.bottom),
                    cv::Scalar(256, 0, 0, 256), 3);
            cv::putText(m_orig_image, obj.label, cv::Point(obj.bbox.left, obj.bbox.top + 12), cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(256, 255, 255));
        }
    }

    void onRender() override
    {
        cv::imshow("ObjDetectApp", m_orig_image);
        cv::waitKey(0);
        cv::destroyAllWindows();
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
        objDetectParams.conf_threshold = 0.25;
        objDetectParams.nms_threshold = 0.45;
        objDetectParams.scale_width = shape.width / m_orig_image.cols;
        objDetectParams.scale_height = shape.height / m_orig_image.rows;
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
    cv::Mat m_orig_image;
};

} // namespace perf_cases
} // namespace bsp_perf

#endif // __OBJDETECTAPP_HPP__