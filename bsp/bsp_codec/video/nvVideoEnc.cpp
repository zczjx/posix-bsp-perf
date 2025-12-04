#include "nvVideoEnc.hpp"
#include <linux/videodev2.h>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <nvbufsurface.h>
#include <v4l2_nv_extensions.h>

#define MAX_PLANES 3

namespace bsp_codec
{

nvVideoEnc::nvVideoEnc()
{
    std::cout << "nvVideoEnc::nvVideoEnc() constructor" << std::endl;
    memset(m_output_plane_fds, 0, sizeof(m_output_plane_fds));
    memset(m_capture_plane_fds, 0, sizeof(m_capture_plane_fds));
}

nvVideoEnc::~nvVideoEnc()
{
    std::cout << "nvVideoEnc::~nvVideoEnc() destructor" << std::endl;
    
    // Note: No capture thread in synchronous mode, so no need to stop it
    
    // Stop streaming on both planes
    if (m_encoder) {
        m_encoder->capture_plane.setStreamStatus(false);
        m_encoder->output_plane.setStreamStatus(false);
    }
    
    // Delete encoder (will cleanup buffers)
    if (m_encoder) {
        delete m_encoder;
        m_encoder = nullptr;
    }
    
    // Clean up buffer pool
    {
        std::lock_guard<std::mutex> lock(m_buffer_pool_mutex);
        m_input_buffer_pool.clear();
        std::cout << "nvVideoEnc: Buffer pool cleaned up" << std::endl;
    }
}

uint32_t nvVideoEnc::getV4L2PixelFormat(const std::string& format)
{
    if (format == "YUV420SP" || format == "NV12") return V4L2_PIX_FMT_NV12M;
    if (format == "YUV420M" || format == "I420") return V4L2_PIX_FMT_YUV420M;

    std::cerr << "Unsupported frame format: " << format << std::endl;
    return 0;
}

int nvVideoEnc::setup(EncodeConfig& cfg)
{
    std::cout << "nvVideoEnc::setup() encoding=" << cfg.encodingType 
              << " fps=" << cfg.fps << " resolution=" << cfg.width << "x" << cfg.height << std::endl;

    m_params.width = cfg.width;
    m_params.height = cfg.height;
    m_params.fps = cfg.fps > 0 ? cfg.fps : 30;
    m_params.encoding_type = cfg.encodingType;
    m_params.frame_format = cfg.frameFormat;
    
    // Determine encoder pixel format
    uint32_t encoder_pixfmt = 0;
    if (m_params.encoding_type == "h264") {
        encoder_pixfmt = V4L2_PIX_FMT_H264;
    } else if (m_params.encoding_type == "h265" || m_params.encoding_type == "hevc") {
        encoder_pixfmt = V4L2_PIX_FMT_H265;
    } else {
        std::cerr << "Unsupported encoding type: " << m_params.encoding_type << std::endl;
        return -1;
    }
    
    uint32_t raw_pixfmt = getV4L2PixelFormat(m_params.frame_format);
    if (raw_pixfmt == 0) {
        std::cerr << "Invalid raw pixel format" << std::endl;
        return -1;
    }
    
    // Create encoder in blocking mode
    std::cout << "Creating NvVideoEncoder in blocking mode" << std::endl;
    m_encoder = NvVideoEncoder::createVideoEncoder("enc0");
    if (!m_encoder) {
        std::cerr << "Could not create NvVideoEncoder" << std::endl;
        return -1;
    }
    
    // Set encoder capture plane format (encoded bitstream) - MUST be set before output plane
    int ret = m_encoder->setCapturePlaneFormat(encoder_pixfmt, m_params.width, 
                                                m_params.height, 2 * 1024 * 1024);
    if (ret < 0) {
        std::cerr << "Error in setting capture plane format" << std::endl;
        return -1;
    }
    
    // Set encoder output plane format (raw YUV input)
    ret = m_encoder->setOutputPlaneFormat(raw_pixfmt, m_params.width, m_params.height);
    if (ret < 0) {
        std::cerr << "Error in setting output plane format" << std::endl;
        return -1;
    }
    
    // Set encoding parameters
    ret = m_encoder->setBitrate(m_params.bitrate);
    if (ret < 0) {
        std::cerr << "Error setting bitrate" << std::endl;
        return -1;
    }
    
    ret = m_encoder->setProfile(m_params.profile);
    if (ret < 0) {
        std::cerr << "Error setting profile" << std::endl;
        return -1;
    }
    
    ret = m_encoder->setLevel(m_params.level);
    if (ret < 0) {
        std::cerr << "Error setting level" << std::endl;
        return -1;
    }
    
    ret = m_encoder->setRateControlMode(m_params.rate_control);
    if (ret < 0) {
        std::cerr << "Error setting rate control mode" << std::endl;
        return -1;
    }
    
    ret = m_encoder->setIFrameInterval(m_params.iframe_interval);
    if (ret < 0) {
        std::cerr << "Error setting I-frame interval" << std::endl;
        return -1;
    }
    
    ret = m_encoder->setIDRInterval(m_params.idr_interval);
    if (ret < 0) {
        std::cerr << "Error setting IDR interval" << std::endl;
        return -1;
    }
    
    ret = m_encoder->setFrameRate(m_params.fps, 1);
    if (ret < 0) {
        std::cerr << "Error setting frame rate" << std::endl;
        return -1;
    }
    
    // Setup output plane (raw YUV input)
    ret = setupOutputPlane();
    if (ret < 0) {
        std::cerr << "Error setting up output plane" << std::endl;
        return -1;
    }
    
    // Setup capture plane (encoded bitstream output)
    ret = setupCapturePlane();
    if (ret < 0) {
        std::cerr << "Error setting up capture plane" << std::endl;
        return -1;
    }

    // Subscribe to EOS event
    ret = m_encoder->subscribeEvent(V4L2_EVENT_EOS, 0, 0);
    if (ret < 0) {
        std::cerr << "Error subscribing to EOS event" << std::endl;
        return -1;
    }

    // Start streaming on both planes
    ret = m_encoder->output_plane.setStreamStatus(true);
    if (ret < 0) {
        std::cerr << "Error starting stream on output plane" << std::endl;
        return -1;
    }

    ret = m_encoder->capture_plane.setStreamStatus(true);
    if (ret < 0) {
        std::cerr << "Error starting stream on capture plane" << std::endl;
        return -1;
    }

    // Queue all capture plane buffers
    for (uint32_t i = 0; i < m_encoder->capture_plane.getNumBuffers(); i++)
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));

        v4l2_buf.index = i;
        v4l2_buf.m.planes = planes;

        ret = m_encoder->capture_plane.qBuffer(v4l2_buf, nullptr);
        if (ret < 0) {
            std::cerr << "Error queuing buffer on capture plane" << std::endl;
            return -1;
        }
    }

    // Note: 使用同步模式，不再需要 capture thread
    // 编码结果会在 encode() 函数中同步获取
    std::cout << "nvVideoEnc: Using synchronous mode, capture results in encode()" << std::endl;

    // Calculate frame size for convenience
    if (raw_pixfmt == V4L2_PIX_FMT_NV12M) {
        m_frame_size = m_params.width * m_params.height * 3 / 2; // NV12: Y + UV/2
    } else if (raw_pixfmt == V4L2_PIX_FMT_YUV420M) {
        m_frame_size = m_params.width * m_params.height * 3 / 2; // I420: Y + U/4 + V/4
    }

    std::cout << "nvVideoEnc::setup() completed successfully, frame_size=" << m_frame_size << std::endl;
    return 0;
}

int nvVideoEnc::setupOutputPlane()
{
    // Request buffers on output plane (MMAP)
    int ret = m_encoder->output_plane.setupPlane(V4L2_MEMORY_MMAP, m_params.num_output_buffers, 
                                                  true, false);
    if (ret < 0) {
        std::cerr << "Error requesting buffers on output plane" << std::endl;
        return -1;
    }

    // Map all output plane buffers
    for (uint32_t i = 0; i < m_encoder->output_plane.getNumBuffers(); i++)
    {
        NvBuffer *buffer = m_encoder->output_plane.getNthBuffer(i);
        ret = buffer->map();
        if (ret < 0) {
            std::cerr << "Error mapping output plane buffer " << i << std::endl;
            return -1;
        }

        // Store FD for reference
        m_output_plane_fds[i] = buffer->planes[0].fd;
    }

    std::cout << "Output plane setup complete, " << m_encoder->output_plane.getNumBuffers() 
              << " buffers allocated" << std::endl;
    return 0;
}

int nvVideoEnc::setupCapturePlane()
{
    // Request buffers on capture plane (MMAP)
    int ret = m_encoder->capture_plane.setupPlane(V4L2_MEMORY_MMAP, m_params.num_capture_buffers, 
                                                   true, false);
    if (ret < 0) {
        std::cerr << "Error requesting buffers on capture plane" << std::endl;
        return -1;
    }

    std::cout << "Capture plane setup complete, " << m_encoder->capture_plane.getNumBuffers() 
              << " buffers allocated" << std::endl;
    return 0;
}

int nvVideoEnc::encode(EncodeInputBuffer& input_buf, EncodePacket& out_pkt)
{
    if (!m_encoder) {
        std::cerr << "Encoder not initialized" << std::endl;
        return -1;
    }

    // Handle EOS
    if (out_pkt.pkt_eos) {
        std::cout << "nvVideoEnc::encode() sending EOS" << std::endl;

        // If initial fill not done yet, wait for it
        while (m_output_buffers_queued < m_encoder->output_plane.getNumBuffers() && !m_eos_sent) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // Dequeue a buffer from output plane
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];
        NvBuffer *buffer = nullptr;

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));
        v4l2_buf.m.planes = planes;

        int ret = m_encoder->output_plane.dqBuffer(v4l2_buf, &buffer, nullptr, 100);
        if (ret < 0) {
            std::cerr << "Error dequeueing buffer for EOS" << std::endl;
            return -1;
        }

        // Queue empty buffer to signal EOS
        for (uint32_t i = 0; i < buffer->n_planes; i++) {
            v4l2_buf.m.planes[i].bytesused = 0;
        }

        ret = m_encoder->output_plane.qBuffer(v4l2_buf, nullptr);
        if (ret < 0) {
            std::cerr << "Error queuing EOS buffer" << std::endl;
            return -1;
        }

        m_eos_sent = true;
        return 0;
    }

    // Normal encoding
    struct v4l2_buffer v4l2_buf;
    struct v4l2_plane planes[MAX_PLANES];
    NvBuffer *buffer = nullptr;

    memset(&v4l2_buf, 0, sizeof(v4l2_buf));
    memset(planes, 0, sizeof(planes));
    v4l2_buf.m.planes = planes;

    // For initial fill, don't dequeue
    uint32_t queued = m_output_buffers_queued.load();
    bool is_initial_fill = (queued < m_encoder->output_plane.getNumBuffers());

    if (!is_initial_fill) {
        // Dequeue buffer from output plane with retry logic
        // IMPORTANT: timeout (EAGAIN) is not an error - it means encoder is busy
        // We should retry until a buffer becomes available
        int retry_count = 0;
        while (true) {
            int ret = m_encoder->output_plane.dqBuffer(v4l2_buf, &buffer, nullptr, 100);
            if (ret == 0) {
                // Success
                std::cout << "[Encoder] Got output buffer index=" << v4l2_buf.index << std::endl;
                break;
            } else if (errno == EAGAIN) {
                // Timeout - encoder is busy, retry
                retry_count++;
                if (retry_count % 10 == 0) {  // Log every 1 second
                    std::cout << "[Encoder] Waiting for encoder to process frames (retry " << retry_count << ")..." << std::endl;
                }
                // Small sleep to avoid busy-waiting
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            } else {
                // Real error
                std::cerr << "[Encoder] ERROR: dqBuffer failed with errno=" << errno << std::endl;
                return -1;
            }
        }
    } else {
        // Initial fill: use Nth buffer directly
        v4l2_buf.index = queued;
        buffer = m_encoder->output_plane.getNthBuffer(queued);
    }

    // Copy input data to buffer (plane by plane, considering stride)
    if (input_buf.input_buf_addr) {
        uint8_t *src = (uint8_t*)input_buf.input_buf_addr;

        // For NV12M: 2 planes (Y, UV)
        // Plane 0: Y plane (width x height, 1 byte per pixel)
        // Plane 1: UV plane (width/2 x height/2, 2 bytes per pixel - interleaved U and V)

        for (uint32_t i = 0; i < buffer->n_planes; i++) {
            NvBuffer::NvBufferPlane &plane = buffer->planes[i];
            uint32_t bytes_per_row = plane.fmt.width * plane.fmt.bytesperpixel;
            uint8_t *dst = plane.data;

            // Copy row by row to handle stride
            for (uint32_t row = 0; row < plane.fmt.height; row++) {
                memcpy(dst, src, bytes_per_row);
                dst += plane.fmt.stride;
                src += bytes_per_row;  // Source has no padding
            }

            plane.bytesused = plane.fmt.stride * plane.fmt.height;
            v4l2_buf.m.planes[i].bytesused = plane.bytesused;
        }

        // Note: In V4L2_MEMORY_MMAP mode, cache coherency is handled by V4L2 driver
        // No need for explicit NvBufSurface sync operations
    } else {
        std::cerr << "No input buffer address provided" << std::endl;
        return -1;
    }

    // Queue buffer to output plane
    int ret = m_encoder->output_plane.qBuffer(v4l2_buf, nullptr);
    if (ret < 0) {
        std::cerr << "Error queueing buffer to output plane" << std::endl;
        return -1;
    }
    std::cout << "[Encoder] Buffer queued to output plane, index=" << v4l2_buf.index << std::endl;

    if (is_initial_fill) {
        m_output_buffers_queued++;
        if (m_output_buffers_queued == m_encoder->output_plane.getNumBuffers()) {
            std::cout << "Initial fill complete, " << m_output_buffers_queued.load() 
                      << " buffers queued" << std::endl;
        }
    }

    // Release the input buffer back to the pool
    // Since we've already copied the data to encoder's buffer, we can release it immediately
    releaseInputBufferToPool(input_buf.input_buf_addr);

    // === 同步模式：等待编码完成并返回结果 ===
    // 如果 out_pkt 有有效的 buffer，等待并填充编码结果
    if (out_pkt.encode_pkt.size() > 0 && out_pkt.max_size > 0) {
        // Initial fill 阶段不等待输出（编码器需要积累几帧才开始输出）
        if (!is_initial_fill || m_output_buffers_queued >= m_encoder->output_plane.getNumBuffers()) {
            struct v4l2_buffer cap_v4l2_buf;
            struct v4l2_plane cap_planes[MAX_PLANES];
            NvBuffer *cap_buffer = nullptr;
            
            memset(&cap_v4l2_buf, 0, sizeof(cap_v4l2_buf));
            memset(cap_planes, 0, sizeof(cap_planes));
            cap_v4l2_buf.m.planes = cap_planes;
            
            std::cout << "[Encoder Sync] Waiting for encoded frame..." << std::endl;
            
            // 阻塞等待编码结果（使用较长的超时时间）
            ret = m_encoder->capture_plane.dqBuffer(cap_v4l2_buf, &cap_buffer, nullptr, 5000);
            if (ret == 0 && cap_buffer && cap_buffer->planes[0].bytesused > 0) {
                size_t encoded_size = cap_buffer->planes[0].bytesused;
                std::cout << "[Encoder Sync] Got encoded frame, size=" << encoded_size << std::endl;
                
                // 拷贝编码数据到 out_pkt
                if (encoded_size <= out_pkt.max_size) {
                    memcpy(out_pkt.encode_pkt.data(), 
                           cap_buffer->planes[0].data, 
                           encoded_size);
                    out_pkt.pkt_len = encoded_size;
                    out_pkt.pkt_eos = (cap_buffer->planes[0].bytesused == 0) ? 1 : 0;
                } else {
                    std::cerr << "[Encoder Sync] ERROR: out_pkt buffer too small (" 
                              << out_pkt.max_size << "), need " << encoded_size << std::endl;
                    out_pkt.pkt_len = 0;
                }
                
                // 通过 callback 也返回（保持兼容性）
                if (m_callback && encoded_size > 0) {
                    m_callback(m_userdata, 
                              (const char*)cap_buffer->planes[0].data, 
                              encoded_size);
                }
                
                // 将 capture buffer 归还
                ret = m_encoder->capture_plane.qBuffer(cap_v4l2_buf, nullptr);
                if (ret < 0) {
                    std::cerr << "[Encoder Sync] Error returning capture buffer" << std::endl;
                }
            } else if (ret < 0 && errno == EAGAIN) {
                std::cerr << "[Encoder Sync] Timeout waiting for encoded frame" << std::endl;
                out_pkt.pkt_len = 0;
            } else if (ret < 0) {
                std::cerr << "[Encoder Sync] ERROR dequeueing capture buffer, errno=" << errno << std::endl;
                out_pkt.pkt_len = 0;
            }
        } else {
            // Initial fill 阶段，还没有输出
            std::cout << "[Encoder] Initial fill in progress (" << m_output_buffers_queued.load() 
                      << "/" << m_encoder->output_plane.getNumBuffers() << "), no output yet" << std::endl;
            out_pkt.pkt_len = 0;
        }
    }

    return 0;
}

int nvVideoEnc::getEncoderHeader(std::string& headBuf)
{
    if (!m_encoder) {
        std::cerr << "Encoder not initialized" << std::endl;
        return -1;
    }

    // For H.264/H.265, get SPS/PPS from first encoded frame's header
    // This is typically done after encoding the first frame
    // For now, return empty (can be filled after first frame if needed)
    headBuf.clear();

    // Alternatively, you can use encoder's getHeaderBuffer if available
    // uint32_t size = 0;
    // char *header = m_encoder->getHeaderBuffer(size);
    // if (header && size > 0) {
    //     headBuf.assign(header, size);
    // }

    return 0;
}

int nvVideoEnc::reset()
{
    std::cout << "nvVideoEnc::reset() called" << std::endl;

    // Note: No capture thread in synchronous mode

    // Stop streaming
    if (m_encoder) {
        m_encoder->capture_plane.setStreamStatus(false);
        m_encoder->output_plane.setStreamStatus(false);
    }

    // Reset state
    m_eos_sent = false;
    m_output_buffers_queued = 0;

    return 0;
}

std::shared_ptr<EncodeInputBuffer> nvVideoEnc::getInputBuffer()
{
    std::lock_guard<std::mutex> lock(m_buffer_pool_mutex);

    // 检查是否有可用的 buffer
    for (auto& info : m_input_buffer_pool) {
        if (!info.in_use) {
            info.in_use = true;
            return info.input_buf;
        }
    }

    // 如果池未满，创建新的 buffer
    if (m_input_buffer_pool.size() < m_buffer_pool_size) {
        InputBufferInfo info;

        // 分配内存（使用 aligned_alloc 以满足硬件对齐要求）
        size_t alignment = 256;  // 256 字节对齐
        void* raw_ptr = aligned_alloc(alignment, m_frame_size);
        if (!raw_ptr) {
            std::cerr << "Failed to allocate buffer of size " << m_frame_size << std::endl;
            return nullptr;
        }

        // 使用自定义 deleter 来正确释放 aligned_alloc 分配的内存
        info.buffer = std::shared_ptr<uint8_t>(
            static_cast<uint8_t*>(raw_ptr),
            [](uint8_t* p) { free(p); }
        );

        // 创建 EncodeInputBuffer
        info.input_buf = std::make_shared<EncodeInputBuffer>();
        info.input_buf->internal_buf = info.buffer;  // 保存引用防止释放
        info.input_buf->input_buf_addr = info.buffer.get();
        info.input_buf->input_buf_fd = -1;  // nvenc 不使用 fd
        info.in_use = true;

        m_input_buffer_pool.push_back(info);

        std::cout << "nvVideoEnc: Created new buffer in pool (total: "
                  << m_input_buffer_pool.size() << ")" << std::endl;

        return info.input_buf;
    }

    // 池已满，等待或返回空
    std::cerr << "nvVideoEnc: Buffer pool exhausted (size: "
              << m_buffer_pool_size << ")" << std::endl;
    return nullptr;
}

size_t nvVideoEnc::getFrameSize()
{
    return m_frame_size;
}

void nvVideoEnc::releaseInputBufferToPool(void* addr)
{
    if (!addr) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_buffer_pool_mutex);

    // Find the buffer in the pool and mark it as not in use
    for (auto& info : m_input_buffer_pool) {
        if (info.input_buf && info.input_buf->input_buf_addr == addr) {
            info.in_use = false;
            return;
        }
    }

    std::cerr << "Warning: Attempted to release buffer not in pool" << std::endl;
}

} // namespace bsp_codec

