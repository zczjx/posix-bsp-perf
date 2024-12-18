#ifndef __VIDEO_DETECT_APP_HPP__
#define __VIDEO_DETECT_APP_HPP__

#include <framework/BasePerfCase.hpp>
#include <shared/BspLogger.hpp>
#include <shared/ArgParser.hpp>
#include <shared/BspFileUtils.hpp>
#include <bsp_dnn/dnnObjDetector.hpp>
#include <bsp_codec/IDecoder.hpp>
#include <bsp_codec/IEncoder.hpp>
#include <memory>
#include <string>
#include <iostream>
#include <vector>

namespace bsp_perf {
namespace perf_cases {
using namespace bsp_dnn;
using namespace bsp_codec;

class VideoDetectApp : public bsp_perf::common::BasePerfCase
{
public:

    VideoDetectApp(bsp_perf::shared::ArgParser&& args):
        BasePerfCase(std::move(args)),
        m_logger{std::make_unique<bsp_perf::shared::BspLogger>("VideoDetectApp")}
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
    VideoDetectApp(const VideoDetectApp&) = delete;
    VideoDetectApp& operator=(const VideoDetectApp&) = delete;
    VideoDetectApp(VideoDetectApp&&) = delete;
    VideoDetectApp& operator=(VideoDetectApp&&) = delete;
    ~VideoDetectApp()
    {
        ;
    }
private:

    void onInit() override
    {
        auto& params = getArgs();
        std::string videoPath;
        params.getOptionVal("--videoPath", videoPath);
        loadVideoFile(videoPath);

        std::string modelPath;
        params.getOptionVal("--modelPath", modelPath);
        m_dnnObjDetector->loadModel(modelPath);

        std::string decType;
        params.getOptionVal("--decoderType", decType);
        setupDecoder(decType);

        params.getOptionVal("--encoderType", m_encoderType);
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
            m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, "{} VideoDetectApp::onProcess() objDetectOutput: bbox: [{}, {}, {}, {}], score: {}, label: {}",
                LOG_TAG, obj.bbox.left, obj.bbox.top, obj.bbox.right, obj.bbox.bottom, obj.score, obj.label);
            cv::rectangle(*m_orig_image_ptr, cv::Point(obj.bbox.left, obj.bbox.top), cv::Point(obj.bbox.right, obj.bbox.bottom),
                    labelColorMap[obj.label], 2);
            cv::putText(*m_orig_image_ptr, obj.label, cv::Point(obj.bbox.left, obj.bbox.top + 12), cv::FONT_HERSHEY_COMPLEX, 0.4, cv::Scalar(256, 255, 255));
        }
    }

    void onRender() override
    {
    }

    void onRelease() override
    {
        m_dnnObjDetector.reset();
    }

private:

    void loadVideoFile(std::string& videoPath)
    {
        auto vformat = BspFileUtils::getFileExtension(videoPath);

        if(vformat.compare("h264") == 0 || vformat.compare("h265") == 0)
        {
            m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, "VideoDetectApp::format: {}", vformat);
        }
        else
        {
            m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Error, "VideoDetectApp::onInit() Unsupported video format: {}", vformat);
            throw std::runtime_error("Unsupported video format.");
        }
        m_videoFileContext = BspFileUtils::LoadFileMmap(videoPath);
    }

    void setupDecoder(std::string& decType)
    {
        m_decoder = IDecoder::create("rkmpp");
    }

    void setObjDetectParams(ObjDetectParams& objDetectParams)
    {
        IDnnEngine::dnnInputShape shape;
        m_dnnObjDetector->getInputShape(shape);
        objDetectParams.model_input_width = shape.width;
        objDetectParams.model_input_height = shape.height;
        objDetectParams.conf_threshold = 0.25;
        objDetectParams.nms_threshold = 0.45;
        objDetectParams.scale_width = shape.width / m_orig_image_ptr->cols;
        objDetectParams.scale_height = shape.height / m_orig_image_ptr->rows;
        objDetectParams.pads.left = 0;
        objDetectParams.pads.right = 0;
        objDetectParams.pads.top = 0;
        objDetectParams.pads.bottom = 0;
        m_dnnObjDetector->getOutputQuantParams(objDetectParams.quantize_zero_points, objDetectParams.quantize_scales);
    }

private:
    std::string m_name {"[VideoDetectApp]:"};
    std::unique_ptr<bsp_perf::shared::BspLogger> m_logger{nullptr};
    std::unique_ptr<bsp_dnn::dnnObjDetector> m_dnnObjDetector{nullptr};
    bsp_dnn::ObjDetectParams m_objDetectParams{};
    std::shared_ptr<BspFileUtils::FileContext> m_videoFileContext{nullptr};

    std::string m_encoderType{""};
    std::unique_ptr<IDecoder> m_decoder{nullptr};
    std::unique_ptr<IDecoder> m_encoder{nullptr};
};


} // namespace perf_cases
} // namespace bsp_perf

#endif // __VIDEO_DETECT_APP_HPP__