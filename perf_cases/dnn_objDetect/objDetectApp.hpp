#ifndef __OBJDETECTAPP_HPP__
#define __OBJDETECTAPP_HPP__

#include <framework/BasePerfCase.hpp>
#include <shared/BspLogger.hpp>
#include <shared/ArgParser.hpp>
#include <bsp_dnn/dnnObjDetector.hpp>
#include <memory>
#include <string>
#include <iostream>

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
    }

    void onProcess() override
    {
        bool ret;
        auto& params = getArgs();
        ret = params.getFlagVal("--imagePath");

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
    std::string m_name {"[ObjDetectApp]:"};
    std::unique_ptr<bsp_perf::shared::BspLogger> m_logger{nullptr};
    std::unique_ptr<bsp_dnn::dnnObjDetector> m_dnnObjDetector{nullptr};
};

} // namespace perf_cases
} // namespace bsp_perf

#endif // __OBJDETECTAPP_HPP__