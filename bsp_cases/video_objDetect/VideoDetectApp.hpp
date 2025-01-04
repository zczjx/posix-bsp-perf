#ifndef __VIDEO_DETECT_APP_HPP__
#define __VIDEO_DETECT_APP_HPP__

#include <framework/BasePerfCase.hpp>
#include <shared/BspLogger.hpp>
#include <shared/ArgParser.hpp>
#include <shared/BspFileUtils.hpp>
#include <bsp_dnn/dnnObjDetector.hpp>
#include <bsp_codec/IDecoder.hpp>
#include <bsp_codec/IEncoder.hpp>
#include <bsp_g2d/IGraphics2D.hpp>
#include <memory>
#include <string>
#include <iostream>
#include <vector>
#include <opencv2/opencv.hpp>

namespace bsp_perf {
namespace perf_cases {
using namespace bsp_dnn;
using namespace bsp_codec;
using namespace bsp_g2d;

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
    ~VideoDetectApp() = default;
private:

    void onInit() override
    {
        auto& params = getArgs();
        std::string inputVideoPath;
        params.getOptionVal("--inputVideoPath", inputVideoPath);
        loadVideoFile(inputVideoPath);

        std::string modelPath;
        params.getOptionVal("--modelPath", modelPath);
        m_dnnObjDetector->loadModel(modelPath);

        std::string g2dPlatform;
        params.getOptionVal("--graphics2D", g2dPlatform);
        m_g2d = IGraphics2D::create(g2dPlatform);

        std::string decType;
        params.getOptionVal("--decoderType", decType);
        setupDecoder(decType);

        params.getOptionVal("--encoderType", m_encoderType);

        std::string outputVideoPath;
        params.getOptionVal("--outputVideoPath", outputVideoPath);
        m_out_fp = std::shared_ptr<FILE>(fopen(outputVideoPath.c_str(), "wb"), fclose);
    }

    void onProcess() override
    {
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, "read video size: {} bytes", m_videoFileContext->size);
        const int PKT_CHUNK_SIZE = 8192;
        uint8_t* pkt_data_start = m_videoFileContext->data.get();
        uint8_t* pkt_data_end = pkt_data_start + m_videoFileContext->size;

        while (m_videoFileContext->size > 0)
        {
            int pkt_eos = 0;
            int chunk_size = PKT_CHUNK_SIZE;

            if (pkt_data_start + chunk_size >= pkt_data_end)
            {
                pkt_eos = 1;
                chunk_size = pkt_data_end - pkt_data_start;
            }

            DecodePacket dec_pkt = {
                .data = pkt_data_start,
                .pkt_size = chunk_size,
                .pkt_eos = pkt_eos,
            };
            m_decoder->decode(dec_pkt);

            pkt_data_start += chunk_size;

            if (pkt_data_start >= pkt_data_end)
            {
                m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Warn, "VideoDetectApp::onProcess() Video file read complete");
                break;
            }
        }
    }

    void onRender() override
    {
        ;
    }

    void onRelease() override
    {
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, "waiting finish");
        std::this_thread::sleep_for(std::chrono::seconds(1));
        fflush(m_out_fp.get());
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, "VideoDetectApp::onRelease() Output video file closed");
        m_out_fp.reset();
        m_dnnObjDetector.reset();
        BspFileUtils::ReleaseFileMmap(m_videoFileContext);
    }

private:
    void loadVideoFile(std::string& videoPath)
    {
        m_decode_format = BspFileUtils::getFileExtension(videoPath);

        if(m_decode_format.compare("h264") == 0 || m_decode_format.compare("h265") == 0)
        {
            m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, "VideoDetectApp::format: {}", m_decode_format);
        }
        else
        {
            m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Error, "VideoDetectApp::onInit() Unsupported video format: {}", m_decode_format);
            throw std::runtime_error("Unsupported video format.");
        }
        m_videoFileContext = BspFileUtils::LoadFileMmap(videoPath);
    }

    void setupDecoder(std::string& decType)
    {
        m_decoder = IDecoder::create(decType);
        DecodeConfig cfg = {
            .encoding = m_decode_format,
            .fps = 30,
        };
        m_decoder->setup(cfg);

        auto decoderCallback = [this](std::any userdata, std::shared_ptr<DecodeOutFrame> frame)
        {
            if (m_encoder == nullptr)
            {
                m_encoder = IEncoder::create(m_encoderType);
                EncodeConfig enc_cfg = {
                    .encodingType = "h264",
                    .frameFormat = "YUV420SP",
                    .fps = 30,
                    .width = frame->width,
                    .height = frame->height,
                    .hor_stride = frame->width_stride,
                    .ver_stride = frame->height_stride,
                };
                int ret = m_encoder->setup(enc_cfg);
                m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Debug, "VideoDetectApp::createG2DBuffer() encoder setup ret: {}", ret);
            }

            auto& objDetectOutput = dnnInference(frame);
            IGraphics2D::G2DBufferParams dec_out_g2d_params = {
                .virtual_addr = frame->virt_addr,
                .width = frame->width,
                .height = frame->height,
                .width_stride = frame->width_stride,
                .height_stride = frame->height_stride,
                .format = frame->format,
            };
            std::shared_ptr<IGraphics2D::G2DBuffer> g2d_dec_out_buf = m_g2d->createG2DBuffer("virtualaddr", dec_out_g2d_params);

            if (m_rgb888_buf.size() != (frame->width * frame->height * 3))
            {
                m_rgb888_buf.resize(frame->width * frame->height * 3);
            }

            if (m_yuv420_buf.size() != (frame->width * frame->height * 3 / 2))
            {
                m_yuv420_buf.resize(frame->width * frame->height * 3 / 2);
            }

            IGraphics2D::G2DBufferParams rgb888_g2d_buf_params = {
                .virtual_addr = m_rgb888_buf.data(),
                .rawBufferSize = frame->width * frame->height * 3,
                .width = frame->width,
                .height = frame->height,
                .width_stride = frame->width,
                .height_stride = frame->height,
                .format = "RGB888",
            };

            std::shared_ptr<IGraphics2D::G2DBuffer> rgb888_g2d_buf = m_g2d->createG2DBuffer("virtualaddr", rgb888_g2d_buf_params);
            int ret = m_g2d->imageCvtColor(g2d_dec_out_buf, rgb888_g2d_buf, frame->format, "RGB888");
            cv::Mat cvRGB888Image(frame->height, frame->width, CV_8UC3, m_rgb888_buf.data());
            int i = 1 + (std::rand() % m_colors_list.size());

            for (const auto& item : objDetectOutput)
            {
                if (m_labelColorMap.find(item.label) == m_labelColorMap.end())
                {
                    m_labelColorMap[item.label] = m_colors_list[i % m_colors_list.size()];
                    i++;
                }
                cv::rectangle(cvRGB888Image, cv::Point(item.bbox.left, item.bbox.top), cv::Point(item.bbox.right, item.bbox.bottom),
                    m_labelColorMap[item.label], 2);
                cv::putText(cvRGB888Image, item.label, cv::Point(item.bbox.left, item.bbox.top + 12), cv::FONT_HERSHEY_COMPLEX, 0.4, cv::Scalar(255, 255, 255));
            }

            IGraphics2D::G2DBufferParams yuv420_g2d_buf_params = {
                .virtual_addr = m_yuv420_buf.data(),
                .rawBufferSize = frame->width * frame->height * 3 / 2,
                .width = frame->width,
                .height = frame->height,
                .width_stride = frame->width_stride,
                .height_stride = frame->height_stride,
                .format = frame->format,
            };

            std::shared_ptr<IGraphics2D::G2DBuffer> yuv420_g2d_buf = m_g2d->createG2DBuffer("virtualaddr", yuv420_g2d_buf_params);
            ret = m_g2d->imageCvtColor(rgb888_g2d_buf, yuv420_g2d_buf, "RGB888", frame->format);
            std::shared_ptr<EncodeInputBuffer> enc_in_buf = m_encoder->getInputBuffer();

            IGraphics2D::G2DBufferParams enc_in_g2d_params = {
                .virtual_addr = enc_in_buf->input_buf_addr,
                .width = frame->width,
                .height = frame->height,
                .width_stride = frame->width,
                .height_stride = frame->height,
                .format = frame->format,
            };
            std::shared_ptr<IGraphics2D::G2DBuffer> g2d_enc_in_buf = m_g2d->createG2DBuffer("virtualaddr", enc_in_g2d_params);
            ret = m_g2d->imageCopy(yuv420_g2d_buf, g2d_enc_in_buf);
            m_frame_index++;
            // Encode to file
            // Write header on first frame->
            if (m_frame_index == 1)
            {
                std::string enc_header;
                m_encoder->getEncoderHeader(enc_header);
                 m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, "VideoDetectApp::onProcess() Write encoder header enc_header.size(): {}", enc_header.size());
                fwrite(enc_header.c_str(), 1, enc_header.size(), m_out_fp.get());
                m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, "VideoDetectApp::onProcess() Write encoder header");
            }

            EncodePacket enc_pkt = {
                .max_size = m_encoder->getFrameSize(),
                .pkt_eos = frame->eos_flag,
                .pkt_len = 0,
            };
            enc_pkt.encode_pkt.resize(enc_pkt.max_size);
            auto encode_len = m_encoder->encode(*enc_in_buf, enc_pkt);
            fwrite(enc_pkt.encode_pkt.data(), 1, enc_pkt.pkt_len, m_out_fp.get());
            m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, "VideoDetectApp::onProcess() Write encoded pkt: {}", m_frame_index);
        };

        m_decoder->setDecodeReadyCallback(decoderCallback, nullptr);
    }

    void setObjDetectParams(ObjDetectParams& objDetectParams, std::shared_ptr<DecodeOutFrame> frame)
    {
        IDnnEngine::dnnInputShape shape;
        m_dnnObjDetector->getInputShape(shape);
        objDetectParams.model_input_width = shape.width;
        objDetectParams.model_input_height = shape.height;
        objDetectParams.model_input_channel = shape.channel;
        objDetectParams.conf_threshold = 0.35;
        objDetectParams.nms_threshold = 0.45;
        objDetectParams.scale_width = static_cast<float>(shape.width) / static_cast<float>(frame->width);
        objDetectParams.scale_height = static_cast<float>(shape.height) / static_cast<float>(frame->height);
        objDetectParams.pads.left = 0;
        objDetectParams.pads.right = 0;
        objDetectParams.pads.top = 0;
        objDetectParams.pads.bottom = 0;
        m_dnnObjDetector->getOutputQuantParams(objDetectParams.quantize_zero_points, objDetectParams.quantize_scales);
    }

    std::vector<ObjDetectOutputBox>& dnnInference(std::shared_ptr<DecodeOutFrame> frame)
    {
        bsp_dnn::ObjDetectInput objDetectInput = {
            .handleType = "DecodeOutFrame",
            .imageHandle = frame,
        };
        m_dnnObjDetector->pushInputData(std::make_shared<bsp_dnn::ObjDetectInput>(objDetectInput));
        setObjDetectParams(m_objDetectParams, frame);
        m_dnnObjDetector->runObjDetect(m_objDetectParams);
        return m_dnnObjDetector->popOutputData();
    }

private:
    std::string m_name {"[VideoDetectApp]:"};
    std::unique_ptr<bsp_perf::shared::BspLogger> m_logger{nullptr};
    std::unique_ptr<bsp_dnn::dnnObjDetector> m_dnnObjDetector{nullptr};
    bsp_dnn::ObjDetectParams m_objDetectParams{};
    std::shared_ptr<BspFileUtils::FileContext> m_videoFileContext{nullptr};
    std::vector<uint8_t> m_rgb888_buf{};
    std::vector<uint8_t> m_yuv420_buf{};

    std::string m_encoderType{""};
    std::unique_ptr<IDecoder> m_decoder{nullptr};
    std::unique_ptr<IEncoder> m_encoder{nullptr};
    std::unique_ptr<IGraphics2D> m_g2d{nullptr};
    std::string m_decode_format{""};
    std::shared_ptr<FILE> m_out_fp{nullptr};
    size_t m_frame_index{0};
    std::map<std::string, cv::Scalar> m_labelColorMap;
    std::vector<cv::Scalar> m_colors_list = {
        cv::Scalar(128, 128, 0),  // Teal
        cv::Scalar(0, 128, 128),   // Aqua
        cv::Scalar(255, 0, 0),    // Blue
        cv::Scalar(0, 0, 255),    // Red
        cv::Scalar(0, 255, 0),    // Green
        cv::Scalar(255, 255, 0),  // Cyan
        cv::Scalar(128, 128, 128), // Gray
        cv::Scalar(192, 192, 192), // Silver
        cv::Scalar(255, 165, 0),   // Orange
        cv::Scalar(255, 20, 147),  // DeepPink
        cv::Scalar(75, 0, 130),    // Indigo
        cv::Scalar(240, 230, 140), // Khaki
        cv::Scalar(255, 0, 255),  // Magenta
        cv::Scalar(128, 0, 128),  // Purple
        cv::Scalar(0, 255, 255),  // Yellow
        cv::Scalar(128, 0, 0),    // Maroon
        cv::Scalar(0, 128, 0),    // Olive
        cv::Scalar(0, 0, 128),    // Navy
        cv::Scalar(173, 216, 230), // LightBlue
        cv::Scalar(255, 182, 193), // LightPink
        cv::Scalar(144, 238, 144), // LightGreen
        cv::Scalar(255, 255, 224)  // LightYellow
    };
};


} // namespace perf_cases
} // namespace bsp_perf

#endif // __VIDEO_DETECT_APP_HPP__