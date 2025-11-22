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
#include <atomic>
#include <mutex>
#include <any>
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

        // 🚀 启动异步推理线程
        m_inference_running = true;
        m_inference_thread = std::thread(&VideoDetectApp::inferenceThreadFunc, this);
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, 
            "VideoDetectApp::onInit() Inference thread started");
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
        // 等待解码器处理完所有帧（解码是异步的）
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, 
            "VideoDetectApp::onProcess() All data sent to decoder, waiting for decoding to complete...");
        
        // 🚀 关键：等待解码完成并入队
        // DNN推理慢会导致队列满，decoder callback阻塞，需要等待足够长的时间
        // 循环等待，直到队列稳定且推理线程处理速度跟上
        size_t last_frame_count = 0;
        int stable_count = 0;
        for (int i = 0; i < 60; i++)  // 最多等待30秒
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            size_t current_count = m_frame_count.load();
            
            if (current_count == last_frame_count)
            {
                stable_count++;
                if (stable_count >= 3)  // 连续3次（1.5秒）没有新帧，认为解码完成
                {
                    m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, 
                        "VideoDetectApp::onProcess() Decoding appears complete, total frames: {}", current_count);
                    break;
                }
            }
            else
            {
                stable_count = 0;
                last_frame_count = current_count;
                if (i % 4 == 0)  // 每2秒打印一次
                {
                    size_t queue_size = 0;
                    {
                        std::lock_guard<std::mutex> lock(m_queue_mutex);
                        queue_size = m_frame_queue.size();
                    }
                    std::cout << "[Main] Processed frames: " << current_count << ", queue size: " << queue_size << std::endl;
                }
            }
        }

        // 停止推理线程接收新帧，等待它处理完队列
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, 
            "VideoDetectApp::onProcess() Setting stop flag, waiting for inference thread to finish...");
        m_inference_running = false;
        m_queue_cv.notify_all();  // 唤醒推理线程（如果在等待）

        if (m_inference_thread.joinable())
        {
            std::cout << "[Main] Waiting for inference thread to finish, current frames: " << m_frame_count.load() << std::endl;
            m_inference_thread.join();  // 等待推理线程处理完所有帧并发送EOS
            std::cout << "[Main] Inference thread joined, all frames processed: " << m_frame_count.load() << std::endl;
        }

        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, 
            "VideoDetectApp::onProcess() All frames processed and encoded, total: {}", m_frame_count.load());
    }

    void onRender() override
    {
        ;
    }

    void onRelease() override
    {
        // 推理线程已经在 onProcess() 中join了，这里只需要清理资源
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, 
            "VideoDetectApp::onRelease() Starting cleanup, inference thread already joined");

        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, 
            "VideoDetectApp::onRelease() Waiting for encoding to finish...");
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, 
            "Total frames submitted: {}, encoded: {}", m_frame_count.load(), m_encoded_frame_count.load());
        // 等待所有帧编码完成
        // 增加等待时间，确保编码器有足够时间处理 EOS 和刷新缓冲区
        for (int i = 0; i < 10; i++)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            if (m_encoded_frame_count >= m_frame_count)
            {
                m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, 
                    "All frames encoded: {}/{}", m_encoded_frame_count.load(), m_frame_count.load());
                break;
            }
            if (i % 2 == 0)
            {
                m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Debug, 
                    "Waiting... encoded: {}/{}", m_encoded_frame_count.load(), m_frame_count.load());
            }
        }
        if (m_out_fp)
        {
            fflush(m_out_fp.get());
            m_out_fp.reset();
            m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, 
                "VideoDetectApp::onRelease() Output video file closed");
        }

        // 先销毁编码器，再销毁其他资源
        m_encoder.reset();
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, 
            "VideoDetectApp::onRelease() Encoder released");

        m_dnnObjDetector.reset();
        BspFileUtils::ReleaseFileMmap(m_videoFileContext);

        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info,
            "VideoDetectApp::onRelease() Resources released, final encoded frames: {}", 
            m_encoded_frame_count.load());
    }

    void onEncodeReady(std::any userdata, const char* data, int size)
    {
        std::cout << "[onEncodeReady] CALLED with size=" << size << ", m_encoded_frame_count=" << m_encoded_frame_count.load() << std::endl;
        if (data && size > 0 && m_out_fp)
        {
            std::lock_guard<std::mutex> lock(m_file_mutex);
            size_t written = fwrite(data, 1, size, m_out_fp.get());

            if (written != (size_t)size)
            {
                m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Error,
                    "VideoDetectApp::onEncodeReady() Write failed, expected: {}, written: {}", size, written);
                return;
            }

            m_encoded_frame_count++;
            std::cout << "zczjx--> onEncodeReady, m_encoded_frame_count: " << m_encoded_frame_count.load() << " (submitted: " << m_frame_count.load() << ")" << std::endl;
            // Log progress every 30 frames
            if (m_encoded_frame_count % 30 == 0)
            {
                m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info,
                    "VideoDetectApp::onEncodeReady() Encoded frame {}, size: {} bytes",
                    m_encoded_frame_count.load(), size);
            }
        }
        else
        {
            std::cout << "[onEncodeReady] WARNING: Invalid data or file pointer!" << std::endl;
        }
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

        auto decoderCallback = [this](std::any /*userdata*/, std::shared_ptr<DecodeOutFrame> frame)
        {
            if (m_encoder == nullptr)
            {
                m_encoder = IEncoder::create(m_encoderType);

                // 修复 stride 为 0 的问题：当 stride 为 0 时，使用 width/height 作为默认值
                int hor_stride = (frame->width_stride > 0) ? frame->width_stride : frame->width;
                int ver_stride = (frame->height_stride > 0) ? frame->height_stride : frame->height;
                EncodeConfig enc_cfg = {
                    .encodingType = "h264",
                    .frameFormat = "YUV420SP",
                    .fps = 30,
                    .width = frame->width,
                    .height = frame->height,
                    .hor_stride = hor_stride,
                    .ver_stride = ver_stride,
                };
                int ret = m_encoder->setup(enc_cfg);
                m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, 
                    "VideoDetectApp encoder setup ret: {}, width: {}, height: {}, hor_stride: {}, ver_stride: {}", 
                    ret, frame->width, frame->height, hor_stride, ver_stride);
                // 设置编码完成回调（参考 EncodeApp.hpp）
                m_encoder->setEncodeReadyCallback(
                    [this](std::any userdata, const char* data, int size) {
                        this->onEncodeReady(userdata, data, size);
                    },
                    std::any(this)
                );

                // 获取并写入编码器头部（如 SPS/PPS）
                std::string enc_header;
                m_encoder->getEncoderHeader(enc_header);
                if (!enc_header.empty())
                {
                    m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, 
                        "VideoDetectApp::onProcess() Write encoder header enc_header.size(): {}", enc_header.size());
                    fwrite(enc_header.c_str(), 1, enc_header.size(), m_out_fp.get());
                    fflush(m_out_fp.get());
                }
            }
            m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Debug, "frame->width: {}, frame->height: {}, frame->width_stride: {}, frame->height_stride: {}, frame->format: {}, frame->valid_data_size: {}", frame->width, frame->height, frame->width_stride, frame->height_stride, frame->format, frame->valid_data_size);

            // ⚠️ 确保编码器在入队前就绪（避免死锁）
            // 推理线程需要编码器，如果编码器在回调中创建但回调被队列满阻塞，就会死锁
            if (m_encoder == nullptr)
            {
                std::cout << "[Decoder] WARNING: Encoder not ready yet, skipping frame for now" << std::endl;
                return;  // 跳过这一帧，等下一帧再创建编码器和入队
            }

            // 🚀 异步架构：快速入队，不阻塞解码器
            // 拷贝帧数据（重要！避免 decoder buffer 被下一帧覆盖）
            FrameTask task;
            task.frame_data.resize(frame->valid_data_size);
            std::memcpy(task.frame_data.data(), frame->virt_addr, frame->valid_data_size);
            task.width = frame->width;
            task.height = frame->height;
            task.format = frame->format;
            task.eos_flag = frame->eos_flag;

            {
                std::unique_lock<std::mutex> lock(m_queue_mutex);
                // 🔧 关键修复：总是等待队列有空间，不要因为停止标志就跳过等待
                // 这样可以确保所有解码的帧都被入队，不会因为推理慢而丢帧
                m_queue_cv.wait(lock, [this] { return m_frame_queue.size() < MAX_QUEUE_SIZE; });

                // 直接入队，不检查 m_inference_running
                m_frame_queue.push(std::move(task));

                // 定期打印队列状态
                if (m_frame_queue.size() % 50 == 0)
                {
                    std::cout << "[Decoder] Frame queued, queue size: " << m_frame_queue.size() << std::endl;
                }
            }
            m_queue_cv.notify_one();  // 唤醒推理线程
            // ✅ 解码器回调立即返回，不阻塞！
            // 推理和编码在独立的推理线程中异步执行
        };

        m_decoder->setDecodeReadyCallback(decoderCallback, nullptr);
    }

    void setObjDetectParams(ObjDetectParams& objDetectParams, std::shared_ptr<DecodeOutFrame> frame)
    {
        IDnnEngine::dnnInputShape shape;
        m_dnnObjDetector->getInputShape(shape);
        auto& params = getArgs();
        objDetectParams.model_input_width = shape.width;
        objDetectParams.model_input_height = shape.height;
        objDetectParams.model_input_channel = shape.channel;
        params.getSubOptionVal("objDetectParams", "--conf_threshold", objDetectParams.conf_threshold);
        params.getSubOptionVal("objDetectParams", "--nms_threshold", objDetectParams.nms_threshold);
        objDetectParams.scale_width = static_cast<float>(shape.width) / static_cast<float>(frame->width);
        objDetectParams.scale_height = static_cast<float>(shape.height) / static_cast<float>(frame->height);
        params.getSubOptionVal("objDetectParams", "--pads_left", objDetectParams.pads.left);
        params.getSubOptionVal("objDetectParams", "--pads_right", objDetectParams.pads.right);
        params.getSubOptionVal("objDetectParams", "--pads_top", objDetectParams.pads.top);
        params.getSubOptionVal("objDetectParams", "--pads_bottom", objDetectParams.pads.bottom);
        m_dnnObjDetector->getOutputQuantParams(objDetectParams.quantize_zero_points, objDetectParams.quantize_scales);
    }

    std::vector<ObjDetectOutputBox> dnnInference(std::shared_ptr<DecodeOutFrame> frame)
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
    std::vector<uint8_t> m_rgba_buf{};  // RGB888 buffer for NvVic conversion and drawing bbox (节省内存，3字节/像素)
    std::vector<uint8_t> m_yuv420_buf{};  // YUV420 buffer for encoder input

    std::string m_encoderType{""};
    std::unique_ptr<IDecoder> m_decoder{nullptr};
    std::unique_ptr<IEncoder> m_encoder{nullptr};
    std::unique_ptr<IGraphics2D> m_g2d{nullptr};
    std::string m_decode_format{""};
    std::shared_ptr<FILE> m_out_fp{nullptr};

    // 帧计数器
    std::atomic<size_t> m_frame_count{0};          // 提交到编码器的帧数
    std::atomic<size_t> m_encoded_frame_count{0};  // 编码完成的帧数
    std::mutex m_file_mutex;                        // 保护文件写入的互斥锁

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

    // 🔧 异步推理架构：解码 → 队列 → 推理线程 → 编码
    // 解决 DNN inference 阻塞解码器的问题
    struct FrameTask {
        std::vector<uint8_t> frame_data;  // 拷贝帧数据（避免 decoder buffer 被覆盖）
        uint32_t width;
        uint32_t height;
        std::string format;
        bool eos_flag;
    };
    std::queue<FrameTask> m_frame_queue;
    std::mutex m_queue_mutex;
    std::condition_variable m_queue_cv;
    std::thread m_inference_thread;
    std::atomic<bool> m_inference_running{false};
    static constexpr size_t MAX_QUEUE_SIZE = 30;  // 最大队列长度

    // 推理线程函数
    void inferenceThreadFunc()
    {
        std::cout << "[Inference Thread] Starting, m_inference_running=" << m_inference_running << std::endl;
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, 
            "VideoDetectApp::inferenceThreadFunc() Inference thread started");

        int loop_count = 0;
        // 🔧 关键修复：继续处理直到队列为空（不要因为 m_inference_running=false 就退出）
        while (true)
        {
            loop_count++;
            if (loop_count % 10 == 1) {
                std::cout << "[Inference Thread] Loop " << loop_count << ", waiting for frames..." << std::endl;
            }
            
            FrameTask task;
            bool has_task = false;
            {
                std::unique_lock<std::mutex> lock(m_queue_mutex);
                std::cout << "[Inference Thread] Before wait, queue size=" << m_frame_queue.size() 
                          << ", m_inference_running=" << m_inference_running << std::endl;
                
                // 等待队列中有任务
                m_queue_cv.wait(lock, [this] { 
                    bool should_wake = !m_frame_queue.empty() || !m_inference_running;
                    return should_wake;
                });
                
                std::cout << "[Inference Thread] After wait, queue size=" << m_frame_queue.size() 
                          << ", m_inference_running=" << m_inference_running << std::endl;
                
                if (!m_frame_queue.empty())
                {
                    task = std::move(m_frame_queue.front());
                    m_frame_queue.pop();
                    has_task = true;
                    std::cout << "[Inference Thread] Got frame from queue, new size=" << m_frame_queue.size() << std::endl;
                }
                else if (!m_inference_running)
                {
                    // 队列为空且收到停止信号，退出线程
                    std::cout << "[Inference Thread] Exiting: queue empty and m_inference_running=false" << std::endl;
                    break;
                }
                else
                {
                    // 假唤醒，继续等待
                    std::cout << "[Inference Thread] Spurious wakeup, continuing..." << std::endl;
                    continue;
                }
            }
            m_queue_cv.notify_all();  // 通知可能等待的 decoder（队列有空间了）

            if (!has_task) {
                continue;  // 应该不会到这里，但为了安全
            }

            // ⚠️ 等待编码器就绪（第一帧会创建编码器）
            int wait_count = 0;
            while (m_encoder == nullptr)
            {
                wait_count++;
                if (wait_count == 1 || wait_count % 10 == 0) {
                    std::cout << "[Inference Thread] Waiting for encoder (attempt " << wait_count << ")..." << std::endl;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));

                // 如果等待超过1秒且收到停止信号，放弃这一帧
                if (wait_count > 100 && !m_inference_running) {
                    std::cout << "[Inference Thread] Encoder not ready after 1s, giving up on this frame" << std::endl;
                    break;
                }
            }

            if (!m_encoder)
            {
                std::cout << "[Inference Thread] Encoder not ready, skipping frame" << std::endl;
                m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Warn,
                    "VideoDetectApp::inferenceThreadFunc() Encoder not ready, skipping frame");
                continue;
            }

            // 执行 DNN 推理（在推理线程中，不阻塞解码器）
            auto frame = std::make_shared<DecodeOutFrame>();
            frame->virt_addr = task.frame_data.data();
            frame->width = task.width;
            frame->height = task.height;
            frame->format = task.format;
            frame->valid_data_size = task.frame_data.size();
            frame->eos_flag = task.eos_flag;

            // 执行 DNN 推理（异步，不阻塞解码器）
            auto objDetectOutput = dnnInference(frame);

            // 🔍 调试：打印检测结果数量
            std::cout << "[Inference Thread] Frame " << m_frame_count.load() 
                      << " detected " << objDetectOutput.size() << " objects" << std::endl;
            for (const auto& item : objDetectOutput)
            {
                std::cout << "  - " << item.label << " @ (" 
                          << item.bbox.left << "," << item.bbox.top << ")-(" 
                          << item.bbox.right << "," << item.bbox.bottom 
                          << ") score=" << item.score << std::endl;
            }

            // 修复 stride 为 0 的问题
            size_t input_width_stride = frame->width_stride > 0 ? 
                                        static_cast<size_t>(frame->width_stride) : 
                                        static_cast<size_t>(frame->width);
            size_t input_height_stride = frame->height_stride > 0 ? 
                                         static_cast<size_t>(frame->height_stride) : 
                                         static_cast<size_t>(frame->height);

            // ⚠️ 关键修复：YUV → RGBA 也需要使用 handle + imageCopy 模式
            // 步骤1: 创建输入 YUV420 帧的 G2DBuffer（virtualaddr 模式）
            IGraphics2D::G2DBufferParams dec_out_g2d_params = {
                .virtual_addr = task.frame_data.data(),
                .rawBufferSize = task.frame_data.size(),  // ⚠️ 关键：必须设置！
                .width = static_cast<size_t>(task.width),
                .height = static_cast<size_t>(task.height),
                .width_stride = input_width_stride,
                .height_stride = input_height_stride,
                .format = task.format,
            };
            std::shared_ptr<IGraphics2D::G2DBuffer> g2d_dec_out_buf = m_g2d->createG2DBuffer("virtualaddr", dec_out_g2d_params);

            // 步骤2: 创建 RGBA8888 handle buffer（让 NvVic 管理，用于接收转换结果）
            size_t rgba_buffer_size = frame->width * frame->height * 4;  // RGBA8888: 4 字节/像素
            if (m_rgba_buf.size() != rgba_buffer_size)
            {
                m_rgba_buf.resize(rgba_buffer_size);
            }

            IGraphics2D::G2DBufferParams rgba_handle_params = {
                .virtual_addr = nullptr,  // ⚠️ handle 模式，必须是 nullptr
                .rawBufferSize = rgba_buffer_size,
                .width = static_cast<size_t>(frame->width),
                .height = static_cast<size_t>(frame->height),
                .width_stride = static_cast<size_t>(frame->width),
                .height_stride = static_cast<size_t>(frame->height),
                .format = "RGBA8888",
            };
            std::shared_ptr<IGraphics2D::G2DBuffer> rgba_handle_buf = m_g2d->createG2DBuffer("handle", rgba_handle_params);

            // 步骤3: 执行 YUV420 到 RGBA8888 的转换（转换到 handle buffer）
            int ret = m_g2d->imageCvtColor(g2d_dec_out_buf, rgba_handle_buf, task.format, "RGBA8888");
            if (ret != 0)
            {
                m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Error,
                    "VideoDetectApp::inferenceThreadFunc() YUV to RGBA conversion failed: {}", ret);
                m_g2d->releaseG2DBuffer(rgba_handle_buf);
                m_g2d->releaseG2DBuffer(g2d_dec_out_buf);
                continue;
            }

            std::cout << "[Inference Thread] YUV to RGBA conversion succeeded" << std::endl;

            // 步骤4: 从 handle buffer 复制到 host buffer（供 OpenCV 使用）
            IGraphics2D::G2DBufferParams rgba_host_params = {
                .virtual_addr = m_rgba_buf.data(),
                .rawBufferSize = rgba_buffer_size,
                .width = static_cast<size_t>(task.width),
                .height = static_cast<size_t>(task.height),
                .width_stride = static_cast<size_t>(task.width),
                .height_stride = static_cast<size_t>(task.height),
                .format = "RGBA8888",
            };
            std::shared_ptr<IGraphics2D::G2DBuffer> rgba_host_buf = m_g2d->createG2DBuffer("virtualaddr", rgba_host_params);
            if (!rgba_host_buf)
            {
                m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Error,
                    "VideoDetectApp::inferenceThreadFunc() Failed to create RGBA host buffer");
                m_g2d->releaseG2DBuffer(rgba_handle_buf);
                m_g2d->releaseG2DBuffer(g2d_dec_out_buf);
                continue;
            }
            
            ret = m_g2d->imageCopy(rgba_handle_buf, rgba_host_buf);
            if (ret != 0)
            {
                m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Error,
                    "VideoDetectApp::inferenceThreadFunc() RGBA copy to host failed: {}", ret);
                m_g2d->releaseG2DBuffer(rgba_host_buf);
                m_g2d->releaseG2DBuffer(rgba_handle_buf);
                m_g2d->releaseG2DBuffer(g2d_dec_out_buf);
                continue;
            }

            std::cout << "[Inference Thread] RGBA copied to host buffer for drawing" << std::endl;

            // 步骤5: 使用 OpenCV 在 RGBA8888 图像上画 bbox
            cv::Mat cvRGBAImage(task.height, task.width, CV_8UC4, m_rgba_buf.data());

            int i = 1 + (std::rand() % m_colors_list.size());

            for (const auto& item : objDetectOutput)
            {
                if (m_labelColorMap.find(item.label) == m_labelColorMap.end())
                {
                    m_labelColorMap[item.label] = m_colors_list[i % m_colors_list.size()];
                    i++;
                }
                cv::rectangle(cvRGBAImage, cv::Point(item.bbox.left, item.bbox.top), cv::Point(item.bbox.right, item.bbox.bottom),
                    m_labelColorMap[item.label], 2);
                cv::putText(cvRGBAImage, item.label, cv::Point(item.bbox.left, item.bbox.top + 12), cv::FONT_HERSHEY_COMPLEX, 0.4, cv::Scalar(255, 255, 255, 255));
            }

            std::cout << "[Inference Thread] Drew " << objDetectOutput.size() << " detection boxes" << std::endl;

            // 步骤6: 将画好框的 RGBA 数据复制回 handle buffer
            // ⚠️ 关键修复：必须重新创建 rgba_host_buf，让 createG2DBuffer 将修改后的 m_rgba_buf 复制到 NvBufSurface
            // 因为 OpenCV 修改的是 m_rgba_buf 内存，而 imageCopy 需要从 NvBufSurface 读取
            // 先释放旧的 rgba_host_buf
            m_g2d->releaseG2DBuffer(rgba_host_buf);
            
            // 重新创建 rgba_host_buf，createG2DBuffer 会自动将画好框的 m_rgba_buf 复制到 NvBufSurface
            rgba_host_buf = m_g2d->createG2DBuffer("virtualaddr", rgba_host_params);
            if (!rgba_host_buf)
            {
                m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Error,
                    "VideoDetectApp::inferenceThreadFunc() Failed to recreate RGBA host buffer after drawing");
                m_g2d->releaseG2DBuffer(rgba_handle_buf);
                m_g2d->releaseG2DBuffer(g2d_dec_out_buf);
                continue;
            }
            
            // 现在复制回 handle buffer（此时 NvBufSurface 中有画好框的数据）
            ret = m_g2d->imageCopy(rgba_host_buf, rgba_handle_buf);
            if (ret != 0)
            {
                m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Error,
                    "VideoDetectApp::inferenceThreadFunc() RGBA copy back to handle failed: {}", ret);
                m_g2d->releaseG2DBuffer(rgba_host_buf);
                m_g2d->releaseG2DBuffer(rgba_handle_buf);
                m_g2d->releaseG2DBuffer(g2d_dec_out_buf);
                continue;
            }
            
            std::cout << "[Inference Thread] RGBA copied back to handle buffer" << std::endl;

            // ⚠️ 立即释放 rgba_host_buf，减少资源占用
            m_g2d->releaseG2DBuffer(rgba_host_buf);

            // 步骤6: 创建 YUV420 handle buffer（让 NvVic 管理，用于接收转换结果）
            size_t yuv420_buffer_size = task.width * task.height * 3 / 2;
            if (m_yuv420_buf.size() != yuv420_buffer_size)
            {
                m_yuv420_buf.resize(yuv420_buffer_size);
            }

            IGraphics2D::G2DBufferParams yuv420_handle_params = {
                .virtual_addr = nullptr,  // ⚠️ handle 模式，必须是 nullptr
                .rawBufferSize = yuv420_buffer_size,
                .width = static_cast<size_t>(task.width),
                .height = static_cast<size_t>(task.height),
                .width_stride = input_width_stride,
                .height_stride = input_height_stride,
                .format = task.format,
            };
            std::shared_ptr<IGraphics2D::G2DBuffer> yuv420_handle_buf = m_g2d->createG2DBuffer("handle", yuv420_handle_params);

            // 步骤7: 将画好框的 RGBA8888 转换回 YUV420（从 handle buffer 转换到 handle buffer）
            ret = m_g2d->imageCvtColor(rgba_handle_buf, yuv420_handle_buf, "RGBA8888", task.format);
            if (ret != 0)
            {
                m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Error,
                    "VideoDetectApp::inferenceThreadFunc() RGBA to YUV conversion failed: {}", ret);
                m_g2d->releaseG2DBuffer(yuv420_handle_buf);
                m_g2d->releaseG2DBuffer(rgba_handle_buf);
                m_g2d->releaseG2DBuffer(g2d_dec_out_buf);
                continue;
            }

            std::cout << "[Inference Thread] RGBA to YUV conversion succeeded" << std::endl;

            // 步骤8: ⚠️ 关键！将 YUV 数据从 handle buffer 复制到用户 buffer（供编码器使用）
            IGraphics2D::G2DBufferParams yuv420_host_params = {
                .virtual_addr = m_yuv420_buf.data(),
                .rawBufferSize = yuv420_buffer_size,
                .width = static_cast<size_t>(task.width),
                .height = static_cast<size_t>(task.height),
                .width_stride = input_width_stride,
                .height_stride = input_height_stride,
                .format = task.format,
            };
            std::shared_ptr<IGraphics2D::G2DBuffer> yuv420_host_buf = m_g2d->createG2DBuffer("virtualaddr", yuv420_host_params);
            if (!yuv420_host_buf)
            {
                m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Error,
                    "VideoDetectApp::inferenceThreadFunc() Failed to create YUV host buffer");
                m_g2d->releaseG2DBuffer(yuv420_handle_buf);
                m_g2d->releaseG2DBuffer(rgba_handle_buf);
                m_g2d->releaseG2DBuffer(g2d_dec_out_buf);
                continue;
            }

            ret = m_g2d->imageCopy(yuv420_handle_buf, yuv420_host_buf);
            if (ret != 0)
            {
                m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Error,
                    "VideoDetectApp::inferenceThreadFunc() YUV copy to host failed: {}", ret);
                m_g2d->releaseG2DBuffer(yuv420_host_buf);
                m_g2d->releaseG2DBuffer(yuv420_handle_buf);
                m_g2d->releaseG2DBuffer(rgba_handle_buf);
                m_g2d->releaseG2DBuffer(g2d_dec_out_buf);
                continue;
            }

            std::cout << "[Inference Thread] YUV copied to host buffer for encoder" << std::endl;

            // 步骤9: 释放所有 G2D buffers（避免资源泄漏）
            // ⚠️ 关键优化：按创建的逆序释放，并立即释放不再使用的 buffer
            m_g2d->releaseG2DBuffer(yuv420_host_buf);
            m_g2d->releaseG2DBuffer(yuv420_handle_buf);
            m_g2d->releaseG2DBuffer(rgba_handle_buf);
            m_g2d->releaseG2DBuffer(g2d_dec_out_buf);

            // 获取编码器输入缓冲区
            std::shared_ptr<EncodeInputBuffer> enc_in_buf = m_encoder->getInputBuffer();
            if (!enc_in_buf)
            {
                m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Error,
                    "VideoDetectApp::inferenceThreadFunc() Failed to get input buffer");
                continue;
            }

            // 将画好 bbox 的 YUV420 数据传给编码器进行压缩编码
            enc_in_buf->input_buf_addr = m_yuv420_buf.data();

            // 准备编码输出包
            EncodePacket enc_pkt = {
                .max_size = m_encoder->getFrameSize() * 2,
                .pkt_eos = task.eos_flag,
                .pkt_len = 0,
            };
            enc_pkt.encode_pkt.resize(enc_pkt.max_size);

            // 执行编码
            int encode_ret = m_encoder->encode(*enc_in_buf, enc_pkt);
            if (encode_ret != 0)
            {
                m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Error, 
                    "VideoDetectApp::inferenceThreadFunc() encode failed, ret: {}", encode_ret);
                continue;
            }

            m_frame_count++;
            if (m_frame_count % 30 == 0)
            {
                m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, 
                    "VideoDetectApp::inferenceThreadFunc() Processed {} frames", m_frame_count.load());
            }
        }

        // 🛑 所有帧处理完毕，发送EOS给编码器
        std::cout << "[Inference Thread] All frames processed, sending EOS to encoder" << std::endl;
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, 
            "VideoDetectApp::inferenceThreadFunc() All frames processed ({}), sending EOS to encoder", 
            m_frame_count.load());

        if (m_encoder)
        {
            std::shared_ptr<EncodeInputBuffer> inputBuf = m_encoder->getInputBuffer();
            if (inputBuf)
            {
                EncodePacket eosPkt;
                eosPkt.max_size = m_encoder->getFrameSize() * 2;
                eosPkt.pkt_eos = 1;  // EOS标志
                eosPkt.pkt_len = 0;
                eosPkt.encode_pkt.resize(eosPkt.max_size);
                int ret = m_encoder->encode(*inputBuf, eosPkt);
                std::cout << "[Inference Thread] EOS sent to encoder, ret=" << ret << std::endl;
                m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, 
                    "VideoDetectApp::inferenceThreadFunc() EOS sent to encoder, ret: {}", ret);
            }
            else
            {
                std::cout << "[Inference Thread] ERROR: Failed to get input buffer for EOS" << std::endl;
                m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Error, 
                    "VideoDetectApp::inferenceThreadFunc() Failed to get input buffer for EOS");
            }
        }

        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, 
            "VideoDetectApp::inferenceThreadFunc() Inference thread exiting, processed {} frames", m_frame_count.load());
    }
};


} // namespace perf_cases
} // namespace bsp_perf

#endif // __VIDEO_DETECT_APP_HPP__