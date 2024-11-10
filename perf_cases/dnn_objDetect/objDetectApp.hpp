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
#include "opencv2/core/core.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/imgproc.hpp"

namespace bsp_perf {
namespace perf_cases {

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

    }

    void onRender() override
    {
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Warn, "{} ObjDetectApp::onRender()", LOG_TAG);
        m_logger->printFileLog(bsp_perf::shared::BspLogger::LogLevel::Warn, "{} ObjDetectApp::onRender()", LOG_TAG);
        m_logger->printAsyncFileLog(bsp_perf::shared::BspLogger::LogLevel::Warn, "{} ObjDetectApp::onRender()", LOG_TAG);
    }

    void onRelease() override
    {
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, "{} ObjDetectApp::onRelease()", LOG_TAG);
        m_logger->printFileLog(bsp_perf::shared::BspLogger::LogLevel::Info, "{} ObjDetectApp::onRelease()", LOG_TAG);
        m_logger->printAsyncFileLog(bsp_perf::shared::BspLogger::LogLevel::Info, "{} ObjDetectApp::onRelease()", LOG_TAG);
    }

private:
    void setObjDetectParams(bsp_dnn::ObjDetectParams& objDetectParams)
    {
        objDetectParams.model_input_width = 640;
        objDetectParams.model_input_height = 640;
        objDetectParams.conf_threshold = 0.5;
        objDetectParams.nms_threshold = 0.5;
        objDetectParams.scale_width = 1.0;
        objDetectParams.scale_height = 1.0;
        objDetectParams.pads.left = 0;
        objDetectParams.pads.right = 0;
        objDetectParams.pads.top = 0;
        objDetectParams.pads.bottom = 0;
        objDetectParams.quantize_zero_points = {0, 0, 0};
        objDetectParams.quantize_scales = {1.0, 1.0, 1.0};
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