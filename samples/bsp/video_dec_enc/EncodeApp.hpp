#ifndef __ENCODEAPP_HPP__
#define __ENCODEAPP_HPP__

#include <framework/BasePerfCase.hpp>
#include <shared/BspLogger.hpp>
#include <shared/ArgParser.hpp>
#include <bsp_codec/IEncoder.hpp>
#include <memory>
#include <string>
#include <iostream>
#include <vector>
#include <shared/BspFileUtils.hpp>
#include <bsp_g2d/BytesPerPixel.hpp>

namespace bsp_perf {
namespace perf_cases {
using namespace bsp_codec;
using namespace bsp_perf::shared;

class EncodeApp : public bsp_perf::common::BasePerfCase
{
public:
    EncodeApp(bsp_perf::shared::ArgParser&& args):
        BasePerfCase(std::move(args)),
        m_logger{std::make_unique<bsp_perf::shared::BspLogger>("EncodeApp")}
    {
        m_logger->setPattern();
        auto& params = getArgs();
        std::string encoderImpl;
        params.getOptionVal("--encoderImpl", encoderImpl);
        m_encoder = bsp_codec::IEncoder::create(encoderImpl);
    }

    EncodeApp(const EncodeApp&) = delete;
    EncodeApp& operator=(const EncodeApp&) = delete;
    EncodeApp(EncodeApp&&) = delete;
    EncodeApp& operator=(EncodeApp&&) = delete;

    ~EncodeApp()
    {
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, "EncodeApp::~EncodeApp()");
    }

private:
    void onInit() override
    {
        auto& params = getArgs();
        std::string inputYuvPath;
        std::string outputVideoPath;
        std::string encodingType;
        std::string frameFormat;
        int fps;
        uint32_t width, height;

        params.getOptionVal("--input", inputYuvPath);
        params.getOptionVal("--output", outputVideoPath);
        params.getOptionVal("--encodingType", encodingType);
        params.getOptionVal("--frameFormat", frameFormat);
        params.getOptionVal("--fps", fps);
        params.getOptionVal("--width", width);
        params.getOptionVal("--height", height);

        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info,
            "EncodeApp::onInit() inputYuvPath: {}, outputVideoPath: {}, encodingType: {}, format: {}, fps: {}, resolution: {}x{}",
            inputYuvPath, outputVideoPath, encodingType, frameFormat, fps, width, height);

        // 使用 mmap 加载输入文件
        m_inputFileCtx = BspFileUtils::LoadFileMmap(inputYuvPath);
        if (!m_inputFileCtx || !m_inputFileCtx->data)
        {
            m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Error,
                "EncodeApp::onInit() Failed to load input file: {}", inputYuvPath);
            throw std::runtime_error("Failed to load input file");
        }

        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info,
            "EncodeApp::onInit() Input file loaded, size: {} bytes", m_inputFileCtx->size);

        // 打开输出文件
        m_outputFile = std::shared_ptr<FILE>(fopen(outputVideoPath.c_str(), "wb"), fclose);
        if (!m_outputFile)
        {
            m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Error,
                "EncodeApp::onInit() Failed to open output file: {}", outputVideoPath);
            throw std::runtime_error("Failed to open output file");
        }

        // 配置编码器
        EncodeConfig cfg;
        cfg.encodingType = encodingType;
        cfg.frameFormat = frameFormat;
        cfg.fps = fps;
        cfg.width = width;
        cfg.height = height;
        cfg.hor_stride = width;  // 可以根据实际情况调整
        cfg.ver_stride = height; // 可以根据实际情况调整

        int ret = m_encoder->setup(cfg);
        if (ret != 0)
        {
            m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Error,
                "EncodeApp::onInit() Failed to setup encoder, ret: {}", ret);
            throw std::runtime_error("Failed to setup encoder");
        }
        // 获取并写入编码器头部（如 SPS/PPS）
        std::string header;
        ret = m_encoder->getEncoderHeader(header);
        if (ret == 0 && !header.empty())
        {
            fwrite(header.data(), 1, header.size(), m_outputFile.get());
            fflush(m_outputFile.get());
            m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info,
                "EncodeApp::onInit() Written encoder header, size: {} bytes", header.size());
        }

        // 获取每帧的大小
        // m_frame_size = m_encoder->getFrameSize();
        m_frame_size = width * height * bsp_g2d::BytesPerPixel::getInstance().getBytesPerPixel(frameFormat);
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info,
            "EncodeApp::onInit() Frame size: {} bytes", m_frame_size);

        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info,
            "EncodeApp::onInit() Encoder initialized successfully");
    }

    void onProcess() override
    {
        if (!m_inputFileCtx || !m_inputFileCtx->data)
        {
            m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Error,
                "EncodeApp::onProcess() Input file not loaded");
            return;
        }

        uint8_t* data_ptr = m_inputFileCtx->data.get();
        size_t total_size = m_inputFileCtx->size;
        size_t offset = 0;

        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info,
            "EncodeApp::onProcess() Starting encode, total size: {} bytes, frame_size: {} bytes",
            total_size, m_frame_size);

        while (offset + m_frame_size <= total_size)
        {
            // 获取编码器输入缓冲区
            std::shared_ptr<EncodeInputBuffer> inputBuf = m_encoder->getInputBuffer();
            if (!inputBuf)
            {
                m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Error,
                    "EncodeApp::onProcess() Failed to get input buffer");
                break;
            }

            // 拷贝输入数据到编码器缓冲区
            // 新的统一API：编码器分配并管理内存，用户拷贝数据到 input_buf_addr
            memcpy(inputBuf->input_buf_addr, data_ptr + offset, m_frame_size);

            // 准备输出包
            EncodePacket outPkt;
            outPkt.max_size = m_frame_size * 2; // 预留足够空间
            outPkt.pkt_eos = 0;
            outPkt.encode_pkt.resize(outPkt.max_size);

            // 编码
            m_encoder->encode(*inputBuf, outPkt);
            m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info,
                "EncodeApp::onProcess() Encoded {} frames, offset: {} / {}",
                m_frame_count, offset, total_size);
            fwrite(outPkt.encode_pkt.data(), 1, outPkt.pkt_len, m_outputFile.get());

            m_frame_count++;
            offset += m_frame_size;

            // Log progress every 30 frames
            if (m_frame_count % 30 == 0)
            {
                m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info,
                    "EncodeApp::onProcess() Encoded {} frames, offset: {} / {}",
                    m_frame_count, offset, total_size);
            }

            // Small delay to prevent overwhelming the encoder
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        // 发送 EOS
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info,
            "EncodeApp::onProcess() Sending EOS after {} frames", m_frame_count);

        std::shared_ptr<EncodeInputBuffer> inputBuf = m_encoder->getInputBuffer();
        if (inputBuf)
        {
            EncodePacket eosPkt;
            eosPkt.max_size = m_encoder->getFrameSize() * 2;
            eosPkt.pkt_eos = 1;
            eosPkt.pkt_len = 0;
            eosPkt.encode_pkt.resize(eosPkt.max_size);
            m_encoder->encode(*inputBuf, eosPkt);
            fwrite(eosPkt.encode_pkt.data(), 1, eosPkt.pkt_len, m_outputFile.get());
        }

        // Wait for encoding to complete
        std::this_thread::sleep_for(std::chrono::seconds(1));

        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info,
            "EncodeApp::onProcess() Encode process finished, total frames: {}, encoded: {}",
            m_frame_count, m_encoded_frame_count.load());
    }

    void onRender() override
    {
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, "EncodeApp::onRender()");
    }

    void onRelease() override
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        if (m_outputFile)
        {
            fflush(m_outputFile.get());
            m_outputFile.reset();
        }

        m_encoder->tearDown();
        m_encoder.reset();

        if (m_inputFileCtx)
        {
            BspFileUtils::ReleaseFileMmap(m_inputFileCtx);
        }

        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info,
            "EncodeApp::onRelease() Resources released");
    }

private:
    std::string m_name {"[EncodeApp]:"};
    std::unique_ptr<bsp_perf::shared::BspLogger> m_logger{nullptr};
    std::unique_ptr<IEncoder> m_encoder{nullptr};
    std::shared_ptr<BspFileUtils::FileContext> m_inputFileCtx{nullptr};
    std::shared_ptr<FILE> m_outputFile{nullptr};
    size_t m_frame_size{0};
    std::atomic<size_t> m_frame_count{0};
    std::atomic<size_t> m_encoded_frame_count{0};
};

} // namespace perf_cases
} // namespace bsp_perf

#endif // __ENCODEAPP_HPP__

