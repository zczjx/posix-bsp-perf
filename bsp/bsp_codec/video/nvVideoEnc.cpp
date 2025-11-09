#include "nvVideoEnc.hpp"
#include <linux/videodev2.h>
#include <iostream>
#include <cstring>
#include <unistd.h>
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
    
    // Stop capture thread
    m_stop_thread = true;
    if (m_capture_thread.joinable()) {
        m_capture_thread.join();
    }
    
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
    
    // Start capture thread
    m_stop_thread = false;
    m_capture_thread = std::thread(&nvVideoEnc::captureThreadFunc, this);
    
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

void nvVideoEnc::captureThreadFunc()
{
    std::cout << "nvVideoEnc::captureThreadFunc() started" << std::endl;
    
    while (!m_stop_thread)
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];
        NvBuffer *buffer = nullptr;
        
        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));
        v4l2_buf.m.planes = planes;
        
        // Dequeue buffer from capture plane (blocking)
        int ret = m_encoder->capture_plane.dqBuffer(v4l2_buf, &buffer, nullptr, -1);
        if (ret < 0) {
            if (errno == EAGAIN) {
                continue;
            }
            std::cerr << "Error dequeueing buffer from capture plane" << std::endl;
            break;
        }
        
        // Check for EOS
        if (buffer->planes[0].bytesused == 0) {
            std::cout << "Got EOS on capture plane" << std::endl;
            m_eos_sent = true;
            // Still queue buffer back
            m_encoder->capture_plane.qBuffer(v4l2_buf, nullptr);
            break;
        }
        
        // Call user callback if set
        if (m_callback && buffer->planes[0].bytesused > 0) {
            m_callback(m_userdata, (const char*)buffer->planes[0].data, buffer->planes[0].bytesused);
        }
        
        // Queue buffer back to capture plane
        ret = m_encoder->capture_plane.qBuffer(v4l2_buf, nullptr);
        if (ret < 0) {
            std::cerr << "Error queueing buffer back to capture plane" << std::endl;
            break;
        }
    }
    
    std::cout << "nvVideoEnc::captureThreadFunc() exiting" << std::endl;
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
        // Dequeue buffer from output plane
        int ret = m_encoder->output_plane.dqBuffer(v4l2_buf, &buffer, nullptr, 100);
        if (ret < 0) {
            std::cerr << "Error dequeueing buffer from output plane" << std::endl;
            return -1;
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
        
        // Sync for device
        for (uint32_t i = 0; i < buffer->n_planes; i++) {
            NvBufSurface *nvbuf_surf = nullptr;
            int ret = NvBufSurfaceFromFd(buffer->planes[i].fd, (void**)(&nvbuf_surf));
            if (ret == 0 && nvbuf_surf) {
                NvBufSurfaceSyncForDevice(nvbuf_surf, 0, i);
            }
        }
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
    
    if (is_initial_fill) {
        m_output_buffers_queued++;
        if (m_output_buffers_queued == m_encoder->output_plane.getNumBuffers()) {
            std::cout << "Initial fill complete, " << m_output_buffers_queued.load() 
                      << " buffers queued" << std::endl;
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
    
    // Stop threads
    m_stop_thread = true;
    if (m_capture_thread.joinable()) {
        m_capture_thread.join();
    }
    
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
    // Create a simple input buffer with virtual address
    // The actual buffer management is handled by the encoder's output plane
    auto buf = std::make_shared<EncodeInputBuffer>();
    buf->input_buf_fd = -1;
    buf->input_buf_addr = nullptr; // Will be filled by caller
    return buf;
}

size_t nvVideoEnc::getFrameSize()
{
    return m_frame_size;
}

} // namespace bsp_codec

