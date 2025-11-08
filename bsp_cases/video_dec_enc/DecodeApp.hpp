#ifndef __DECODEAPP_HPP__
#define __DECODEAPP_HPP__

#include <framework/BasePerfCase.hpp>
#include <shared/BspLogger.hpp>
#include <shared/ArgParser.hpp>
#include <bsp_codec/IDecoder.hpp>
#include <shared/BspFileUtils.hpp>
#include <memory>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <mutex>
#include <atomic>
#include <cstring>

namespace bsp_perf {
namespace perf_cases {
using namespace bsp_codec;
using namespace bsp_perf::shared;

class DecodeApp : public bsp_perf::common::BasePerfCase
{
public:
    DecodeApp(bsp_perf::shared::ArgParser&& args):
        BasePerfCase(std::move(args)),
        m_logger{std::make_unique<bsp_perf::shared::BspLogger>("DecodeApp")}
    {
        m_logger->setPattern();
        auto& params = getArgs();
        std::string decoderImpl;
        params.getOptionVal("--decoderImpl", decoderImpl);
        m_decoder = bsp_codec::IDecoder::create(decoderImpl);
    }

    DecodeApp(const DecodeApp&) = delete;
    DecodeApp& operator=(const DecodeApp&) = delete;
    DecodeApp(DecodeApp&&) = delete;
    DecodeApp& operator=(DecodeApp&&) = delete;

    ~DecodeApp()
    {
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, "DecodeApp::~DecodeApp()");
    }

private:
    std::shared_ptr<BspFileUtils::FileContext> loadVideoFile(std::string& videoPath)
    {
        m_decode_format = BspFileUtils::getFileExtension(videoPath);

        if(m_decode_format.compare("h264") == 0 || m_decode_format.compare("h265") == 0)
        {
            m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, "Video Input File::format: {}", m_decode_format);
        }
        else
        {
            m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Error, "DecodeApp::onInit() Unsupported video format: {}", m_decode_format);
            throw std::runtime_error("Unsupported video format.");
        }
        return BspFileUtils::LoadFileMmap(videoPath);
    }
    void setupDecoder(std::string& decoderImpl, int fps)
    {
        m_decoder = bsp_codec::IDecoder::create(decoderImpl);
            DecodeConfig cfg = {
            .encoding = m_decode_format,
            .fps = fps,
        };
        m_decoder->setup(cfg);
        m_decoder->setDecodeReadyCallback(
            [this](std::any userdata, std::shared_ptr<DecodeOutFrame> frame) {
                this->onDecodeReady(userdata, frame);
            },
            std::any(this)
        );
    }
    void onInit() override
    {
        auto& params = getArgs();

        std::string inputVideoPath;
        params.getOptionVal("--inputVideoPath", inputVideoPath);
        m_inputFileCtx = loadVideoFile(inputVideoPath);

        std::string outputYuvPath;
        params.getOptionVal("--outputVideoPath", outputYuvPath);
        m_out_fp = std::shared_ptr<FILE>(fopen(outputYuvPath.c_str(), "wb"), fclose);

        int fps;
        params.getOptionVal("--fps", fps);
        std::string decoderImpl;
        params.getOptionVal("--decoderImpl", decoderImpl);
        setupDecoder(decoderImpl, fps);

        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info,
            "DecodeApp::onInit() Decoder initialized successfully");
    }

    void onProcess() override
    {
        const int PKT_CHUNK_SIZE = 8192;
        uint8_t* pkt_data_start = m_inputFileCtx->data.get();
        uint8_t* pkt_data_end = pkt_data_start + m_inputFileCtx->size;

        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info,
            "DecodeApp::onProcess() Starting decode, total size: {} bytes", m_inputFileCtx->size);

        while (pkt_data_start < pkt_data_end && !m_decode_error)
        {
            int pkt_eos = 0;
            int chunk_size = PKT_CHUNK_SIZE;

            if (pkt_data_start + chunk_size >= pkt_data_end)
            {
                pkt_eos = 1;
                chunk_size = pkt_data_end - pkt_data_start;
            }

            DecodePacket pkt;
            pkt.data = pkt_data_start;
            pkt.pkt_size = chunk_size;
            pkt.pkt_eos = pkt_eos;

            int ret = m_decoder->decode(pkt);
            if (ret != 0)
            {
                m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Error,
                    "DecodeApp::onProcess() decode failed, ret: {}", ret);
                m_decode_error = true;
                break;
            }

            pkt_data_start += chunk_size;

            if (pkt_eos)
            {
                m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, 
                    "DecodeApp::onProcess() Video file read complete, sent EOS");
                break;
            }
            
            // Small delay to prevent overwhelming the decoder
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        
        // Wait for decoding to complete
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info,
            "DecodeApp::onProcess() Waiting for decoder to finish...");
        
        auto start_wait = std::chrono::steady_clock::now();
        int prev_frame_count = m_frame_count.load();
        
        while (!m_decode_error && std::chrono::steady_clock::now() - start_wait < std::chrono::seconds(10))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            int current_frame_count = m_frame_count.load();
            
            // If no new frames in the last 2 seconds, assume done
            if (current_frame_count == prev_frame_count)
            {
                auto no_frame_duration = std::chrono::steady_clock::now() - start_wait;
                if (no_frame_duration > std::chrono::seconds(2))
                {
                    m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info,
                        "DecodeApp::onProcess() No new frames for 2 seconds, assuming decode complete");
                    break;
                }
            }
            else
            {
                prev_frame_count = current_frame_count;
                start_wait = std::chrono::steady_clock::now(); // Reset timer on new frame
            }
        }
        
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info,
            "DecodeApp::onProcess() Decode process finished, total frames: {}", m_frame_count.load());
    }

    void onRender() override
    {
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, "DecodeApp::onRender()");
    }

    void onRelease() override
    {
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info,
            "DecodeApp::onRelease() Stopping decoder...");

        std::this_thread::sleep_for(std::chrono::seconds(1));
        fflush(m_out_fp.get());
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, "DecodeApp::onRelease() Output video file closed");
        m_decoder.reset();
        BspFileUtils::ReleaseFileMmap(m_inputFileCtx);
        m_out_fp.reset();
    }

    void onDecodeReady(std::any userdata, std::shared_ptr<DecodeOutFrame> frame)
    {
        if (!frame)
        {
            m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Error,
                "DecodeApp::onDecodeReady() frame is null");
            m_decode_error = true;
            return;
        }

        // Check for EOS
        if (frame->eos_flag)
        {
            m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info,
                "DecodeApp::onDecodeReady() Received EOS frame");
            return;
        }

        // 写入 YUV 数据到文件（带线程安全保护）
        if (frame->virt_addr && frame->valid_data_size > 0)
        {
            std::lock_guard<std::mutex> lock(m_file_mutex);
            size_t written = fwrite(frame->virt_addr, 1, frame->valid_data_size, m_out_fp.get());
            
            if (written != frame->valid_data_size)
            {
                m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Error,
                    "DecodeApp::onDecodeReady() Write failed, expected: {}, written: {}", 
                    frame->valid_data_size, written);
                m_decode_error = true;
                return;
            }
            
            m_frame_count++;
            
            // Log progress every 30 frames
            if (m_frame_count % 30 == 0)
            {
                m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info,
                    "DecodeApp::onDecodeReady() Decoded {} frames, current: {}x{}", 
                    m_frame_count.load(), frame->width, frame->height);
            }
        }
        else
        {
            m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Warn,
                "DecodeApp::onDecodeReady() Invalid frame data");
        }
    }

private:
    std::string m_name {"[DecodeApp]:"};
    std::unique_ptr<bsp_perf::shared::BspLogger> m_logger{nullptr};
    std::unique_ptr<IDecoder> m_decoder{nullptr};
    std::shared_ptr<BspFileUtils::FileContext> m_inputFileCtx{nullptr};
    std::shared_ptr<FILE> m_out_fp{nullptr};
    std::string m_decode_format{""};
    std::atomic<size_t> m_frame_count{0};
    std::atomic<bool> m_decode_error{false};
    std::mutex m_file_mutex;  // 保护文件写入的互斥锁
};

} // namespace perf_cases
} // namespace bsp_perf

#endif // __DECODEAPP_HPP__

