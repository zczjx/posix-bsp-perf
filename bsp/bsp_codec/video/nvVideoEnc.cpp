#include "nvVideoEnc.hpp"
#include <iostream>
#include <string.h>
#include <unistd.h>
#include <nvbufsurftransform.h>
#include <linux/videodev2.h>

namespace bsp_codec
{

#define CHECK_NV_ERROR(x) \
    do { \
        int err = x; \
        if (err < 0) { \
            std::cerr << "NVENC Error: " << #x << " failed with error " << err << std::endl; \
            return -1; \
        } \
    } while (0)

nvVideoEnc::nvVideoEnc() : m_encoder(nullptr)
{
    memset(m_output_plane_fds, -1, sizeof(m_output_plane_fds));
    memset(m_capture_plane_fds, -1, sizeof(m_capture_plane_fds));
}

nvVideoEnc::~nvVideoEnc()
{
    m_stop_thread.store(true);
    m_encode_cv.notify_all();
    
    if (m_capture_thread.joinable()) {
        m_capture_thread.join();
    }

    if (m_encoder) {
        // Clean up DMA-BUF file descriptors
        for (uint32_t i = 0; i < m_params.num_output_buffers; i++) {
            if (m_output_plane_fds[i] != -1) {
                m_encoder->output_plane.unmapOutputBuffers(i, m_output_plane_fds[i]);
                NvBufSurf::NvDestroy(m_output_plane_fds[i]);
                m_output_plane_fds[i] = -1;
            }
        }

        for (uint32_t i = 0; i < m_params.num_capture_buffers; i++) {
            if (m_capture_plane_fds[i] != -1) {
                m_encoder->capture_plane.unmapOutputBuffers(i, m_capture_plane_fds[i]);
                NvBufSurface *nvbuf_surf = nullptr;
                if (NvBufSurfaceFromFd(m_capture_plane_fds[i], (void **)(&nvbuf_surf)) >= 0) {
                    NvBufSurfaceDestroy(nvbuf_surf);
                }
                m_capture_plane_fds[i] = -1;
            }
        }

        delete m_encoder;
        m_encoder = nullptr;
    }
}

uint32_t nvVideoEnc::getV4L2PixelFormat(const std::string& format)
{
    if (format == "YUV420SP" || format == "NV12") {
        return V4L2_PIX_FMT_NV12M;
    } else if (format == "YUV420P") {
        return V4L2_PIX_FMT_YUV420M;
    } else if (format == "YUV444P") {
        return V4L2_PIX_FMT_YUV444M;
    } else if (format == "NV24") {
        return V4L2_PIX_FMT_NV24M;
    }
    // Default to NV12M
    return V4L2_PIX_FMT_NV12M;
}

int nvVideoEnc::setup(EncodeConfig& cfg)
{
    m_params.encoding_type = cfg.encodingType;
    m_params.frame_format = cfg.frameFormat;
    m_params.fps = cfg.fps;
    m_params.width = cfg.width;
    m_params.height = cfg.height;
    m_params.bitrate = m_params.width * m_params.height / 8 * m_params.fps; // Auto calculate if not set

    // Create NvVideoEncoder instance
    m_encoder = NvVideoEncoder::createVideoEncoder("nvenc0");
    if (!m_encoder) {
        std::cerr << "Failed to create NvVideoEncoder" << std::endl;
        return -1;
    }

    // Determine encoder pixel format (capture plane - encoded output)
    uint32_t encoder_pixfmt;
    if (m_params.encoding_type == "h264") {
        encoder_pixfmt = V4L2_PIX_FMT_H264;
    } else if (m_params.encoding_type == "h265") {
        encoder_pixfmt = V4L2_PIX_FMT_H265;
    } else {
        std::cerr << "Unsupported encoding type: " << m_params.encoding_type << std::endl;
        return -1;
    }

    // Set encoder capture plane format (encoded output)
    CHECK_NV_ERROR(m_encoder->setCapturePlaneFormat(encoder_pixfmt, m_params.width, 
                                                      m_params.height, 2 * 1024 * 1024));

    // Determine raw pixel format (output plane - raw input)
    uint32_t raw_pixfmt = getV4L2PixelFormat(m_params.frame_format);

    // Set encoder output plane format (raw input)
    CHECK_NV_ERROR(m_encoder->setOutputPlaneFormat(raw_pixfmt, m_params.width, 
                                                     m_params.height, V4L2_COLORSPACE_SMPTE170M));

    // Set bitrate
    CHECK_NV_ERROR(m_encoder->setBitrate(m_params.bitrate));

    // Set profile
    if (m_params.profile != 0) {
        CHECK_NV_ERROR(m_encoder->setProfile(m_params.profile));
    }

    // Set level
    if (m_params.level != (uint32_t)-1) {
        CHECK_NV_ERROR(m_encoder->setLevel(m_params.level));
    }

    // Set rate control mode
    CHECK_NV_ERROR(m_encoder->setRateControlMode(m_params.rate_control));

    // Set I-frame interval
    CHECK_NV_ERROR(m_encoder->setIFrameInterval(m_params.iframe_interval));

    // Set IDR interval
    CHECK_NV_ERROR(m_encoder->setIDRInterval(m_params.idr_interval));

    // Set framerate
    CHECK_NV_ERROR(m_encoder->setFrameRate(m_params.fps, 1));

    // Setup output plane (raw input) with DMABUF
    if (setupOutputPlane() < 0) {
        std::cerr << "Failed to setup output plane" << std::endl;
        return -1;
    }

    // Setup capture plane (encoded output) with MMAP
    if (setupCapturePlane() < 0) {
        std::cerr << "Failed to setup capture plane" << std::endl;
        return -1;
    }

    // Subscribe to End Of Stream event
    CHECK_NV_ERROR(m_encoder->subscribeEvent(V4L2_EVENT_EOS, 0, 0));

    // Set encoder output plane STREAMON
    CHECK_NV_ERROR(m_encoder->output_plane.setStreamStatus(true));

    // Set encoder capture plane STREAMON
    CHECK_NV_ERROR(m_encoder->capture_plane.setStreamStatus(true));

    // Enqueue all empty capture plane buffers
    for (uint32_t i = 0; i < m_encoder->capture_plane.getNumBuffers(); i++) {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));

        v4l2_buf.index = i;
        v4l2_buf.m.planes = planes;

        if (m_encoder->capture_plane.qBuffer(v4l2_buf, NULL) < 0) {
            std::cerr << "Error while queueing buffer at capture plane" << std::endl;
            return -1;
        }
    }

    // Get encoder header (SPS/PPS for H.264/H.265)
    getEncoderHeader(m_encoder_header);

    // Start capture thread
    m_capture_thread = std::thread(&nvVideoEnc::captureThreadFunc, this);

    std::cout << "NVENC Encoder setup complete. " << m_params.width << "x" << m_params.height 
              << " @ " << m_params.fps << "fps, bitrate: " << m_params.bitrate << std::endl;
    
    m_ready_for_encode = true;
    return 0;
}

int nvVideoEnc::setupOutputPlane()
{
    int ret = 0;
    NvBufSurf::NvCommonAllocateParams cParams;

    ret = m_encoder->output_plane.reqbufs(V4L2_MEMORY_DMABUF, m_params.num_output_buffers);
    if (ret) {
        std::cerr << "reqbufs failed for output plane V4L2_MEMORY_DMABUF" << std::endl;
        return ret;
    }

    for (uint32_t i = 0; i < m_encoder->output_plane.getNumBuffers(); i++) {
        cParams.width = m_params.width;
        cParams.height = m_params.height;
        cParams.layout = NVBUF_LAYOUT_PITCH;
        
        // Set color format based on frame format
        if (m_params.frame_format == "NV12" || m_params.frame_format == "YUV420SP") {
            cParams.colorFormat = NVBUF_COLOR_FORMAT_NV12;
        } else if (m_params.frame_format == "YUV420P") {
            cParams.colorFormat = NVBUF_COLOR_FORMAT_YUV420;
        } else {
            cParams.colorFormat = NVBUF_COLOR_FORMAT_NV12; // Default
        }

        cParams.memtag = NvBufSurfaceTag_VIDEO_ENC;
        cParams.memType = NVBUF_MEM_SURFACE_ARRAY;

        int fd;
        ret = NvBufSurf::NvAllocate(&cParams, 1, &fd);
        if (ret < 0) {
            std::cerr << "Failed to create NvBuffer for output plane" << std::endl;
            return ret;
        }
        m_output_plane_fds[i] = fd;

        // Get buffer info to calculate frame size
        NvBufSurface *surf = nullptr;
        if (NvBufSurfaceFromFd(fd, (void**)&surf) == 0) {
            if (i == 0 && surf->numFilled > 0) {
                // Calculate frame size from first buffer
                m_frame_size = 0;
                for (uint32_t j = 0; j < surf->surfaceList[0].planeParams.num_planes; j++) {
                    m_frame_size += surf->surfaceList[0].planeParams.pitch[j] * 
                                   surf->surfaceList[0].planeParams.height[j];
                }
            }
        }
    }

    return ret;
}

int nvVideoEnc::setupCapturePlane()
{
    int ret = m_encoder->capture_plane.setupPlane(V4L2_MEMORY_MMAP, 
                                                    m_params.num_capture_buffers, 
                                                    true, false);
    if (ret < 0) {
        std::cerr << "Could not setup capture plane" << std::endl;
        return ret;
    }

    return 0;
}

int nvVideoEnc::encode(EncodeInputBuffer& input_buf, EncodePacket& out_pkt)
{
    if (!m_encoder || !m_ready_for_encode) {
        std::cerr << "Encoder not ready" << std::endl;
        return -1;
    }

    struct v4l2_buffer v4l2_buf;
    struct v4l2_plane planes[MAX_PLANES];
    NvBuffer *buffer = nullptr;

    memset(&v4l2_buf, 0, sizeof(v4l2_buf));
    memset(planes, 0, sizeof(planes));
    v4l2_buf.m.planes = planes;

    // Dequeue a buffer from output plane
    int ret = m_encoder->output_plane.dqBuffer(v4l2_buf, &buffer, NULL, 10);
    if (ret < 0) {
        if (errno == EAGAIN) {
            // No buffer available, this is not necessarily an error
            return 0;
        }
        std::cerr << "ERROR while DQing buffer at output plane: " << strerror(errno) << std::endl;
        return -1;
    }

    // Handle EOS
    if (out_pkt.pkt_eos) {
        std::cout << "NVENC Encoder: Received EOS packet." << std::endl;
        v4l2_buf.m.planes[0].bytesused = 0;
        m_eos_sent.store(true);
    } else {
        // Map the output plane buffer for the given DMABUF FD
        int buf_fd = input_buf.input_buf_fd;
        if (buf_fd < 0) {
            std::cerr << "Invalid input buffer FD" << std::endl;
            return -1;
        }

        v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        v4l2_buf.memory = V4L2_MEMORY_DMABUF;
        
        // Map the DMABUF
        ret = m_encoder->output_plane.mapOutputBuffers(v4l2_buf, buf_fd);
        if (ret < 0) {
            std::cerr << "Error while mapping buffer at output plane" << std::endl;
            return -1;
        }

        // Sync buffer for device
        NvBufSurface *nvbuf_surf = nullptr;
        for (uint32_t j = 0; j < buffer->n_planes; j++) {
            ret = NvBufSurfaceFromFd(buffer->planes[j].fd, (void**)(&nvbuf_surf));
            if (ret < 0) {
                std::cerr << "Error while NvBufSurfaceFromFd" << std::endl;
                return -1;
            }
            ret = NvBufSurfaceSyncForDevice(nvbuf_surf, 0, j);
            if (ret < 0) {
                std::cerr << "Error while NvBufSurfaceSyncForDevice" << std::endl;
                return -1;
            }
        }

        // Set bytesused for DMABUF
        for (uint32_t j = 0; j < buffer->n_planes; j++) {
            v4l2_buf.m.planes[j].bytesused = buffer->planes[j].bytesused;
        }
    }

    // Queue the buffer for encoding
    ret = m_encoder->output_plane.qBuffer(v4l2_buf, NULL);
    if (ret < 0) {
        std::cerr << "Error while queueing buffer at output plane" << std::endl;
        return -1;
    }

    return 0;
}

void nvVideoEnc::captureThreadFunc()
{
    struct v4l2_buffer v4l2_buf;
    struct v4l2_plane planes[MAX_PLANES];
    NvBuffer *buffer = nullptr;

    while (!m_stop_thread.load()) {
        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));
        v4l2_buf.m.planes = planes;

        // Dequeue encoded buffer from capture plane
        int ret = m_encoder->capture_plane.dqBuffer(v4l2_buf, &buffer, NULL, 100);
        if (ret < 0) {
            if (errno == EAGAIN) {
                continue;
            }
            std::cerr << "Error while DQing buffer from capture plane: " << strerror(errno) << std::endl;
            if (m_encoder->isInError()) {
                std::cout << "Encoder is in error state. Exiting capture thread." << std::endl;
                m_stop_thread.store(true);
            }
            continue;
        }

        // Check for EOS
        if (buffer->planes[0].bytesused == 0) {
            std::cout << "Got 0 size buffer in capture (EOS)" << std::endl;
            if (m_callback) {
                m_callback(m_userdata, nullptr, 0);
            }
            // Re-queue the buffer
            m_encoder->capture_plane.qBuffer(v4l2_buf, NULL);
            m_stop_thread.store(true);
            continue;
        }

        // Call the callback with encoded data
        if (m_callback) {
            m_callback(m_userdata, (const char*)buffer->planes[0].data, buffer->planes[0].bytesused);
        }

        // Re-queue the buffer
        if (m_encoder->capture_plane.qBuffer(v4l2_buf, NULL) < 0) {
            std::cerr << "Error while re-queueing buffer at capture plane" << std::endl;
        }
    }

    std::cout << "NVENC Encoder: Capture thread stopped." << std::endl;
}

int nvVideoEnc::getEncoderHeader(std::string& headBuf)
{
    if (!m_encoder) {
        return -1;
    }

    // For H.264/H.265, get SPS/PPS
    if (m_params.encoding_type == "h264" || m_params.encoding_type == "h265") {
        // Note: NvVideoEncoder doesn't have a direct API to get header separately
        // The header is typically included with the first IDR frame
        // For this implementation, we'll store it when we encode the first frame
        headBuf = m_encoder_header;
        return headBuf.length();
    }

    return 0;
}

int nvVideoEnc::reset()
{
    if (!m_encoder) {
        return -1;
    }

    // Stop threads
    m_stop_thread.store(true);
    m_encode_cv.notify_all();
    
    if (m_capture_thread.joinable()) {
        m_capture_thread.join();
    }

    // Reset encoder state
    m_ready_for_encode = false;
    m_eos_sent.store(false);

    // Re-setup the encoder
    EncodeConfig current_cfg;
    current_cfg.encodingType = m_params.encoding_type;
    current_cfg.frameFormat = m_params.frame_format;
    current_cfg.fps = m_params.fps;
    current_cfg.width = m_params.width;
    current_cfg.height = m_params.height;
    current_cfg.hor_stride = m_params.width;
    current_cfg.ver_stride = m_params.height;

    m_stop_thread.store(false);
    return setup(current_cfg);
}

std::shared_ptr<EncodeInputBuffer> nvVideoEnc::getInputBuffer()
{
    if (!m_encoder || m_output_plane_fds[0] == -1) {
        return nullptr;
    }

    std::shared_ptr<EncodeInputBuffer> inputBuffer = std::make_shared<EncodeInputBuffer>();
    
    // Return the first available output buffer FD
    // In a real implementation, you might want to manage a pool of buffers
    inputBuffer->input_buf_fd = m_output_plane_fds[0];
    
    // Map the buffer to get virtual address
    NvBufSurface *surf = nullptr;
    if (NvBufSurfaceFromFd(m_output_plane_fds[0], (void**)&surf) == 0) {
        if (NvBufSurfaceMap(surf, 0, 0, NVBUF_MAP_READ_WRITE) == 0) {
            inputBuffer->input_buf_addr = (void*)surf->surfaceList[0].mappedAddr.addr[0];
        }
    }

    return inputBuffer;
}

size_t nvVideoEnc::getFrameSize()
{
    return m_frame_size;
}

} // namespace bsp_codec

