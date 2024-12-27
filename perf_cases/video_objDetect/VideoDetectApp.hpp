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
    ~VideoDetectApp()
    {
        ;
    }
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
            // for (const auto& item : objDetectOutput)
            // {
            //     m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Debug,
            //         "Detected object: label={}, score={}, x={}, y={}, width={}, height={}",
            //         item.label, item.score, item.bbox.left, item.bbox.right, item.bbox.top, item.bbox.bottom);
            // }
            IGraphics2D::G2DBufferParams dec_out_g2d_params = {
                .fd = frame->fd,
                .width = frame->width,
                .height = frame->height,
                .width_stride = frame->width_stride,
                .height_stride = frame->height_stride,
                .format = frame->format,
            };
            std::shared_ptr<IGraphics2D::G2DBuffer> g2d_dec_out_buf = m_g2d->createG2DBuffer("fd", dec_out_g2d_params);
            std::shared_ptr<EncodeInputBuffer> enc_in_buf = m_encoder->getInputBuffer();

            IGraphics2D::G2DBufferParams enc_in_g2d_params = {
                .fd = enc_in_buf->input_buf_fd,
                .width = frame->width,
                .height = frame->height,
                .width_stride = frame->width_stride,
                .height_stride = frame->height_stride,
                .format = frame->format,
            };
            std::shared_ptr<IGraphics2D::G2DBuffer> g2d_enc_in_buf = m_g2d->createG2DBuffer("fd", enc_in_g2d_params);

            m_g2d->imageCopy(g2d_dec_out_buf, g2d_enc_in_buf);

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
            m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, "VideoDetectApp::onProcess() Write encoded encode_len: {} enc_pkt.pkt_len: {}", encode_len, enc_pkt.pkt_len);
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
        objDetectParams.model_input_channel = 3;
        objDetectParams.conf_threshold = 0.25;
        objDetectParams.nms_threshold = 0.45;
        objDetectParams.scale_width = shape.width / frame->width;
        objDetectParams.scale_height = shape.height / frame->height;
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

    std::string m_encoderType{""};
    std::unique_ptr<IDecoder> m_decoder{nullptr};
    std::unique_ptr<IEncoder> m_encoder{nullptr};
    std::unique_ptr<IGraphics2D> m_g2d{nullptr};
    std::string m_decode_format{""};
    std::shared_ptr<FILE> m_out_fp{nullptr};
    size_t m_frame_index{0};
};


} // namespace perf_cases
} // namespace bsp_perf

#endif // __VIDEO_DETECT_APP_HPP__