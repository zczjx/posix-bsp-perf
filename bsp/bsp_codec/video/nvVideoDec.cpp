#include "nvVideoDec.hpp"
#include <linux/videodev2.h>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <nvbufsurface.h>
#include <nvbufsurftransform.h>
#include <v4l2_nv_extensions.h>

namespace bsp_codec
{

nvVideoDec::~nvVideoDec()
{
    std::cout << "nvVideoDec::~nvVideoDec() destructor" << std::endl;

    // Stop threads first
    m_running = false;
    m_got_eos = true;

    // Wake up threads
    m_queue_cv.notify_all();

    // Wait for threads to finish with timeout
    if (m_decoder_thread && m_decoder_thread->joinable()) {
        m_decoder_thread->join();
    }
    if (m_capture_thread && m_capture_thread->joinable()) {
        m_capture_thread->join();
    }

    // Stop streaming on capture plane if it's running
    if (m_dec && m_capture_plane_started) {
        m_dec->capture_plane.setStreamStatus(false);
    }

    // Stop streaming on output plane
    if (m_dec) {
        m_dec->output_plane.setStreamStatus(false);
    }

    // Cleanup dst buffer
    if (m_dst_dma_fd != -1)
    {
        NvBufSurface *nvbuf_surf = nullptr;
        int ret = NvBufSurfaceFromFd(m_dst_dma_fd, (void**)(&nvbuf_surf));
        if (ret == 0 && nvbuf_surf != nullptr) {
            NvBufSurfaceDestroy(nvbuf_surf);
        }
        m_dst_dma_fd = -1;
    }
    
    // Cleanup DMA buffers - just close the FDs, decoder will handle cleanup
    for (int i = 0; i < m_num_capture_buffers; i++)
    {
        m_dmabuf_fds[i] = 0;
    }
    
    // Delete decoder
    if (m_dec) {
        delete m_dec;
        m_dec = nullptr;
    }
}

uint32_t nvVideoDec::getV4L2PixelFormat(const std::string& encoding)
{
    if (encoding == "h264") return V4L2_PIX_FMT_H264;
    if (encoding == "h265" || encoding == "hevc") return V4L2_PIX_FMT_H265;
    if (encoding == "vp8") return V4L2_PIX_FMT_VP8;
    if (encoding == "vp9") return V4L2_PIX_FMT_VP9;
    if (encoding == "mpeg2") return V4L2_PIX_FMT_MPEG2;
    if (encoding == "mpeg4") return V4L2_PIX_FMT_MPEG4;
    
    std::cerr << "Unsupported encoding: " << encoding << std::endl;
    return 0;
}

int nvVideoDec::setup(DecodeConfig& cfg)
{
    std::cout << "nvVideoDec::setup() encoding=" << cfg.encoding << " fps=" << cfg.fps << std::endl;
    
    m_params.fps = cfg.fps > 0 ? cfg.fps : 30;
    m_params.decoder_pixfmt = getV4L2PixelFormat(cfg.encoding);
    
    if (m_params.decoder_pixfmt == 0) {
        std::cerr << "Invalid decoder pixel format" << std::endl;
        return -1;
    }
    
    // Create decoder in blocking mode
    std::cout << "Creating NvVideoDecoder in blocking mode" << std::endl;
    m_dec = NvVideoDecoder::createVideoDecoder("dec0");
    if (!m_dec) {
        std::cerr << "Could not create NvVideoDecoder" << std::endl;
        return -1;
    }
    
    // Set output plane format (encoded bitstream)
    int ret = m_dec->setOutputPlaneFormat(m_params.decoder_pixfmt, 4 * 1024 * 1024);
    if (ret < 0) {
        std::cerr << "Error in setting output plane format" << std::endl;
        return -1;
    }
    
    // Set frame input mode to NAL unit mode
    if (m_params.input_nalu) {
        std::cout << "Setting frame input mode to NAL unit mode" << std::endl;
        ret = m_dec->setFrameInputMode(1);
        if (ret < 0) {
            std::cerr << "Error in setting frame input mode" << std::endl;
            return -1;
        }
    }
    
    // Request buffers on output plane
    ret = m_dec->output_plane.setupPlane(V4L2_MEMORY_MMAP, m_params.num_output_buffers, true, false);
    if (ret < 0) {
        std::cerr << "Error in setting up output plane" << std::endl;
        return -1;
    }
    
    // Subscribe to resolution change event
    ret = m_dec->subscribeEvent(V4L2_EVENT_RESOLUTION_CHANGE, 0, 0);
    if (ret < 0) {
        std::cerr << "Error in subscribing to resolution change event" << std::endl;
        return -1;
    }
    
    // Start streaming on output plane
    ret = m_dec->output_plane.setStreamStatus(true);
    if (ret < 0) {
        std::cerr << "Error in starting stream on output plane" << std::endl;
        return -1;
    }
    
    std::cout << "nvVideoDec::setup() completed successfully" << std::endl;
    return 0;
}

int nvVideoDec::startDecoding()
{
    std::cout << "nvVideoDec::startDecoding() - Starting decoder threads" << std::endl;
    
    m_running = true;
    m_got_error = false;
    m_got_eos = false;
    
    // Start capture thread
    m_capture_thread = std::make_unique<std::thread>(&nvVideoDec::captureThreadFunc, this);
    
    // Start decoder thread
    m_decoder_thread = std::make_unique<std::thread>(&nvVideoDec::decoderThreadFunc, this);
    
    std::cout << "nvVideoDec::startDecoding() - Threads started successfully" << std::endl;
    return 0;
}

int nvVideoDec::queryAndSetCapture()
{
    int ret = 0;
    struct v4l2_format format;
    struct v4l2_crop crop;
    int32_t min_dec_capture_buffers;
    
    std::cout << "nvVideoDec::queryAndSetCapture()" << std::endl;
    
    // Get capture plane format from decoder
    ret = m_dec->capture_plane.getFormat(format);
    if (ret < 0) {
        std::cerr << "Error getting format from capture plane" << std::endl;
        return -1;
    }
    
    // Get crop rectangle
    ret = m_dec->capture_plane.getCrop(crop);
    if (ret < 0) {
        std::cerr << "Error getting crop from capture plane" << std::endl;
        return -1;
    }
    
    std::cout << "Video Resolution: " << crop.c.width << "x" << crop.c.height << std::endl;
    m_display_width = crop.c.width;
    m_display_height = crop.c.height;
    m_video_width = format.fmt.pix_mp.width;
    m_video_height = format.fmt.pix_mp.height;
    
    // Destroy old transform buffer if exists
    if (m_dst_dma_fd != -1)
    {
        NvBufSurface *dst_nvbuf_surf = nullptr;
        int ret_temp = NvBufSurfaceFromFd(m_dst_dma_fd, (void**)&dst_nvbuf_surf);
        if (ret_temp == 0) {
            NvBufSurfaceDestroy(dst_nvbuf_surf);
        }
        m_dst_dma_fd = -1;
    }
    
    // Create PitchLinear output buffer for transform
    NvBufSurfaceAllocateParams params = {{0}};
    NvBufSurface *dst_nvbuf_surf = nullptr;
    
    params.params.memType = NVBUF_MEM_SURFACE_ARRAY;
    params.params.width = crop.c.width;
    params.params.height = crop.c.height;
    params.params.layout = NVBUF_LAYOUT_PITCH;
    params.params.colorFormat = NVBUF_COLOR_FORMAT_NV12;
    params.memtag = NvBufSurfaceTag_VIDEO_CONVERT;
    
    ret = NvBufSurfaceAllocate(&dst_nvbuf_surf, 1, &params);
    if (ret < 0)
    {
        std::cerr << "Error allocating destination buffer" << std::endl;
        return -1;
    }
    dst_nvbuf_surf->numFilled = 1;
    m_dst_dma_fd = dst_nvbuf_surf->surfaceList[0].bufferDesc;
    
    // Get minimum number of buffers for capture plane
    ret = m_dec->getMinimumCapturePlaneBuffers(min_dec_capture_buffers);
    if (ret < 0) {
        std::cerr << "Error getting minimum capture plane buffers" << std::endl;
        return -1;
    }
    
    m_num_capture_buffers = min_dec_capture_buffers + m_params.extra_cap_plane_buffer;
    
    // Allocate DMA buffers for capture plane (BlockLinear - hardware requirement)
    NvBufSurfaceAllocateParams capParams = {{0}};
    NvBufSurface *cap_nvbuf_surf = nullptr;
    
    capParams.params.memType = NVBUF_MEM_SURFACE_ARRAY;
    capParams.params.width = crop.c.width;
    capParams.params.height = crop.c.height;
    capParams.params.layout = NVBUF_LAYOUT_BLOCK_LINEAR;
    capParams.params.colorFormat = NVBUF_COLOR_FORMAT_NV12;
    capParams.memtag = NvBufSurfaceTag_VIDEO_DEC;
    
    for (int i = 0; i < m_num_capture_buffers; i++)
    {
        ret = NvBufSurfaceAllocate(&cap_nvbuf_surf, 1, &capParams);
        if (ret < 0)
        {
            std::cerr << "Error allocating capture buffer " << i << std::endl;
            return -1;
        }
        cap_nvbuf_surf->numFilled = 1;
        m_dmabuf_fds[i] = cap_nvbuf_surf->surfaceList[0].bufferDesc;
    }
    
    // Set capture plane format (pass the format structure directly)
    ret = m_dec->capture_plane.setFormat(format);
    if (ret < 0) {
        std::cerr << "Error setting capture plane format" << std::endl;
        return -1;
    }
    
    // Request buffers on capture plane
    ret = m_dec->capture_plane.reqbufs(V4L2_MEMORY_DMABUF, m_num_capture_buffers);
    if (ret < 0) {
        std::cerr << "Error requesting buffers on capture plane" << std::endl;
        return -1;
    }
    
    // Enqueue all buffers on capture plane
    for (int i = 0; i < m_num_capture_buffers; i++)
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];
        
        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));
        
        v4l2_buf.index = i;
        v4l2_buf.m.planes = planes;
        v4l2_buf.m.planes[0].m.fd = m_dmabuf_fds[i];
        v4l2_buf.m.planes[1].m.fd = m_dmabuf_fds[i];
        
        ret = m_dec->capture_plane.qBuffer(v4l2_buf, nullptr);
        if (ret < 0) {
            std::cerr << "Error queueing buffer on capture plane" << std::endl;
            return -1;
        }
    }
    
    // Start streaming on capture plane
    ret = m_dec->capture_plane.setStreamStatus(true);
    if (ret < 0) {
        std::cerr << "Error starting stream on capture plane" << std::endl;
        return -1;
    }
    
    m_capture_plane_started = true;
    std::cout << "Query and set capture successful" << std::endl;
    
    return 0;
}

void nvVideoDec::captureThreadFunc()
{
    std::cout << "nvVideoDec::captureThreadFunc() started" << std::endl;
    
    struct v4l2_event ev;
    int ret;
    
    // Wait for resolution change event
    do {
        ret = m_dec->dqEvent(ev, 50000);
        if (ret < 0) {
            if (errno == EAGAIN) {
                std::cerr << "Timeout waiting for resolution change event" << std::endl;
            } else {
                std::cerr << "Error dequeueing event" << std::endl;
            }
            m_got_error = true;
            return;
        }
    } while (ev.type != V4L2_EVENT_RESOLUTION_CHANGE && !m_got_error);
    
    // Query and set capture plane
    if (queryAndSetCapture() < 0) {
        m_got_error = true;
        return;
    }
    
    // Allocate frame buffer for output (contiguous YUV420SP data)
    size_t frame_buffer_size = m_display_width * m_display_height * 3 / 2;
    std::vector<uint8_t> m_frame_buffer(frame_buffer_size);
    
    // Main capture loop
    while (m_running && !m_got_error && !m_got_eos)
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];
        NvBuffer* buffer = nullptr;
        
        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));
        v4l2_buf.m.planes = planes;
        
        // Check for another resolution change event
        ret = m_dec->dqEvent(ev, 0);
        if (ret == 0 && ev.type == V4L2_EVENT_RESOLUTION_CHANGE) {
            if (queryAndSetCapture() < 0) {
                m_got_error = true;
                break;
            }
            continue;
        }
        
        // Dequeue buffer from capture plane
        ret = m_dec->capture_plane.dqBuffer(v4l2_buf, &buffer, nullptr, -1);
        if (ret < 0) {
            if (errno == EAGAIN) {
                usleep(1000);
                continue;
            }
            std::cerr << "Error dequeueing buffer from capture plane" << std::endl;
            m_got_error = true;
            break;
        }
        
        // Check for EOS
        if (buffer->planes[0].bytesused == 0) {
            std::cout << "Got EOS from capture plane" << std::endl;
            m_got_eos = true;
            break;
        }
        
        // Transform from BlockLinear to PitchLinear
        NvBufSurfTransformRect src_rect, dest_rect;
        src_rect.top = 0;
        src_rect.left = 0;
        src_rect.width = m_display_width;
        src_rect.height = m_display_height;
        dest_rect.top = 0;
        dest_rect.left = 0;
        dest_rect.width = m_display_width;
        dest_rect.height = m_display_height;
        
        NvBufSurfTransformParams transform_params;
        memset(&transform_params, 0, sizeof(transform_params));
        transform_params.transform_flag = NVBUFSURF_TRANSFORM_FILTER;
        transform_params.transform_flip = NvBufSurfTransform_None;
        transform_params.transform_filter = NvBufSurfTransformInter_Nearest;
        transform_params.src_rect = &src_rect;
        transform_params.dst_rect = &dest_rect;
        
        NvBufSurface *src_nvbuf_surf = nullptr;
        NvBufSurface *dst_nvbuf_surf = nullptr;
        
        ret = NvBufSurfaceFromFd(m_dmabuf_fds[v4l2_buf.index], (void**)&src_nvbuf_surf);
        if (ret != 0) {
            std::cerr << "Failed to get source NvBufSurface" << std::endl;
            m_got_error = true;
            break;
        }
        
        ret = NvBufSurfaceFromFd(m_dst_dma_fd, (void**)&dst_nvbuf_surf);
        if (ret != 0) {
            std::cerr << "Failed to get dest NvBufSurface" << std::endl;
            m_got_error = true;
            break;
        }

        ret = NvBufSurfTransform(src_nvbuf_surf, dst_nvbuf_surf, &transform_params);
        if (ret != 0)
        {
            std::cerr << "Transform failed" << std::endl;
            // Still queue buffer back
        }
        else if (m_callback != nullptr)
        {
            std::cout << "Transform succeeded, processing frame..." << std::endl;
            
            // Call user callback with decoded frame
            std::shared_ptr<DecodeOutFrame> frame = std::make_shared<DecodeOutFrame>();

            // Map the transformed buffer to get virtual address
            NvBufSurface *surf = nullptr;
            ret = NvBufSurfaceFromFd(m_dst_dma_fd, (void**)&surf);
            if (ret == 0 && surf != nullptr)
            {
                // Map Y plane
                ret = NvBufSurfaceMap(surf, 0, 0, NVBUF_MAP_READ);
                if (ret == 0)
                {
                    frame->width = m_display_width;
                    frame->height = m_display_height;
                    frame->format = "YUV420SP";  // NV12
                    frame->fd = m_dst_dma_fd;
                    frame->eos_flag = 0;

                    uint8_t* y_src = (uint8_t*)surf->surfaceList[0].mappedAddr.addr[0];
                    uint32_t y_pitch = surf->surfaceList[0].planeParams.pitch[0];
                    uint32_t y_width = surf->surfaceList[0].planeParams.width[0];
                    uint32_t y_height = surf->surfaceList[0].planeParams.height[0];
                    
                    std::cout << "Y plane: " << y_width << "x" << y_height << ", pitch=" << y_pitch << std::endl;
                    
                    if (y_src != nullptr)
                    {
                        // Copy Y plane (row by row to remove padding)
                        uint8_t* y_dst = m_frame_buffer.data();
                        for (uint32_t i = 0; i < y_height; i++)
                        {
                            memcpy(y_dst + i * y_width, 
                                   y_src + i * y_pitch, 
                                   y_width);
                        }
                    }
                    
                    NvBufSurfaceUnMap(surf, 0, 0);
                    
                    // Map UV plane
                    ret = NvBufSurfaceMap(surf, 0, 1, NVBUF_MAP_READ);
                    if (ret == 0)
                    {
                        uint8_t* uv_src = (uint8_t*)surf->surfaceList[0].mappedAddr.addr[1];
                        uint32_t uv_pitch = surf->surfaceList[0].planeParams.pitch[1];
                        uint32_t uv_width = surf->surfaceList[0].planeParams.width[1];
                        uint32_t uv_height = surf->surfaceList[0].planeParams.height[1];
                        uint32_t uv_bytesPerPix = surf->surfaceList[0].planeParams.bytesPerPix[1];
                        
                        std::cout << "UV plane: " << uv_width << "x" << uv_height << ", pitch=" << uv_pitch << ", bpp=" << uv_bytesPerPix << std::endl;
                        
                        if (uv_src != nullptr)
                        {
                            // Copy UV plane (row by row to remove padding)
                            // NV12: UV is interleaved, each row is width * bytesPerPix bytes
                            uint8_t* uv_dst = m_frame_buffer.data() + m_display_width * m_display_height;
                            uint32_t uv_bytes_per_row = uv_width * uv_bytesPerPix;
                            
                            for (uint32_t i = 0; i < uv_height; i++)
                            {
                                memcpy(uv_dst + i * uv_bytes_per_row,
                                       uv_src + i * uv_pitch,
                                       uv_bytes_per_row);
                            }
                        }
                        
                        NvBufSurfaceUnMap(surf, 0, 1);
                        
                        // Now we have complete frame data
                        frame->virt_addr = m_frame_buffer.data();
                        frame->valid_data_size = frame_buffer_size;
                        
                        std::cout << "Calling user callback with frame data, size=" << frame_buffer_size << std::endl;
                        
                        // Call user callback
                        m_callback(m_userdata, frame);
                    }
                    else
                    {
                        std::cerr << "Failed to map UV plane, ret=" << ret << std::endl;
                    }
                }
                else
                {
                    std::cerr << "Failed to map Y plane, ret=" << ret << std::endl;
                }
            }
            else
            {
                std::cerr << "Failed to get NvBufSurface from fd, ret=" << ret << std::endl;
            }
        }
        else
        {
            std::cerr << "Callback is null!" << std::endl;
        }
        
        // Queue buffer back to capture plane
        v4l2_buf.m.planes[0].m.fd = m_dmabuf_fds[v4l2_buf.index];
        v4l2_buf.m.planes[1].m.fd = m_dmabuf_fds[v4l2_buf.index];
        
        ret = m_dec->capture_plane.qBuffer(v4l2_buf, nullptr);
        if (ret < 0) {
            std::cerr << "Error queueing buffer back to capture plane" << std::endl;
            m_got_error = true;
            break;
        }
    }
    
    std::cout << "nvVideoDec::captureThreadFunc() exiting" << std::endl;
}

void nvVideoDec::decoderThreadFunc()
{
    std::cout << "nvVideoDec::decoderThreadFunc() started" << std::endl;
    
    uint32_t num_buffers = m_dec->output_plane.getNumBuffers();
    bool initial_fill = true;
    int ret;
    
    // Initial fill: queue all output plane buffers WITHOUT waiting for capture plane
    if (initial_fill)
    {
        std::cout << "Starting initial fill of " << num_buffers << " buffers" << std::endl;
        
        for (uint32_t i = 0; i < num_buffers && m_running; i++)
        {
            struct v4l2_buffer v4l2_buf;
            struct v4l2_plane planes[MAX_PLANES];
            NvBuffer* buffer;
            
            memset(&v4l2_buf, 0, sizeof(v4l2_buf));
            memset(planes, 0, sizeof(planes));
            
            v4l2_buf.index = i;
            v4l2_buf.m.planes = planes;
            
            buffer = m_dec->output_plane.getNthBuffer(i);
            
            // Get packet from queue with timeout
            PacketInfo pkt;
            {
                std::unique_lock<std::mutex> lock(m_queue_mutex);
                auto timeout = std::chrono::milliseconds(100);
                if (!m_queue_cv.wait_for(lock, timeout, [this] { 
                    return !m_packet_queue.empty() || !m_running || m_got_eos; 
                })) {
                    // Timeout - continue to next iteration
                    std::cout << "Initial fill: waiting for data..." << std::endl;
                    i--; // Retry this buffer
                    continue;
                }
                
                if (!m_running || m_packet_queue.empty()) {
                    break;
                }
                
                pkt = std::move(m_packet_queue.front());
                m_packet_queue.pop();
            }
            
            // Copy data to buffer
            if (pkt.is_eos) {
                buffer->planes[0].bytesused = 0;
            } else {
                size_t copy_size = std::min(pkt.data.size(), (size_t)buffer->planes[0].length);
                memcpy(buffer->planes[0].data, pkt.data.data(), copy_size);
                buffer->planes[0].bytesused = copy_size;
            }
            
            v4l2_buf.m.planes[0].bytesused = buffer->planes[0].bytesused;
            
            std::cout << "Initial fill: queuing buffer " << i << ", size=" << buffer->planes[0].bytesused << std::endl;
            
            ret = m_dec->output_plane.qBuffer(v4l2_buf, nullptr);
            if (ret < 0) {
                std::cerr << "Error queuing buffer on output plane" << std::endl;
                m_got_error = true;
                break;
            }
            
            if (pkt.is_eos) {
                m_got_eos = true;
                break;
            }
        }
        
        initial_fill = false;
        std::cout << "Initial fill completed, switching to normal dqBuffer mode" << std::endl;
    }
    
    // Main decode loop
    while (m_running && !m_got_error && !m_got_eos)
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];
        NvBuffer* buffer;
        
        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));
        v4l2_buf.m.planes = planes;
        
        // Dequeue buffer from output plane
        ret = m_dec->output_plane.dqBuffer(v4l2_buf, &buffer, nullptr, -1);
        if (ret < 0) {
            std::cerr << "Error dequeueing buffer from output plane" << std::endl;
            m_got_error = true;
            break;
        }
        
        // Get packet from queue
        PacketInfo pkt;
        {
            std::unique_lock<std::mutex> lock(m_queue_mutex);
            auto timeout = std::chrono::milliseconds(100);
            if (!m_queue_cv.wait_for(lock, timeout, [this] { 
                return !m_packet_queue.empty() || !m_running || m_got_eos; 
            })) {
                // Timeout - queue empty buffer to signal EOS
                if (m_got_eos) {
                    v4l2_buf.m.planes[0].bytesused = 0;
                    m_dec->output_plane.qBuffer(v4l2_buf, nullptr);
                    break;
                }
                // Otherwise just requeue the buffer and continue
                v4l2_buf.m.planes[0].bytesused = 0;
                m_dec->output_plane.qBuffer(v4l2_buf, nullptr);
                continue;
            }
            
            if (!m_running) {
                break;
            }
            
            if (!m_packet_queue.empty()) {
                pkt = std::move(m_packet_queue.front());
                m_packet_queue.pop();
            } else {
                // Queue empty buffer
                v4l2_buf.m.planes[0].bytesused = 0;
                m_dec->output_plane.qBuffer(v4l2_buf, nullptr);
                continue;
            }
        }
        
        // Copy data to buffer
        if (pkt.is_eos) {
            buffer->planes[0].bytesused = 0;
            m_got_eos = true;
        } else {
            size_t copy_size = std::min(pkt.data.size(), (size_t)buffer->planes[0].length);
            memcpy(buffer->planes[0].data, pkt.data.data(), copy_size);
            buffer->planes[0].bytesused = copy_size;
        }
        
        v4l2_buf.m.planes[0].bytesused = buffer->planes[0].bytesused;
        
        // Queue buffer back to output plane
        ret = m_dec->output_plane.qBuffer(v4l2_buf, nullptr);
        if (ret < 0) {
            std::cerr << "Error queuing buffer back to output plane" << std::endl;
            m_got_error = true;
            break;
        }
    }
    
    std::cout << "nvVideoDec::decoderThreadFunc() exiting" << std::endl;
}

int nvVideoDec::decode(DecodePacket& pkt_data)
{
    // Start threads on first decode call
    if (!m_running) {
        int ret = startDecoding();
        if (ret < 0) {
            return -1;
        }
    }
    
    // Add packet to queue
    PacketInfo pkt;
    if (pkt_data.pkt_eos) {
        pkt.is_eos = true;
    } else {
        pkt.data.resize(pkt_data.pkt_size);
        memcpy(pkt.data.data(), pkt_data.data, pkt_data.pkt_size);
        pkt.is_eos = false;
    }

    {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        m_packet_queue.push(std::move(pkt));
    }
    m_queue_cv.notify_one();

    return 0;
}

int nvVideoDec::tearDown()
{
    std::cout << "nvVideoDec::tearDown()" << std::endl;

    m_running = false;
    m_got_eos = true;
    m_queue_cv.notify_all();

    // Wait for threads
    if (m_decoder_thread && m_decoder_thread->joinable()) {
        m_decoder_thread->join();
    }
    if (m_capture_thread && m_capture_thread->joinable()) {
        m_capture_thread->join();
    }

    // Clear packet queue
    {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        std::queue<PacketInfo> empty;
        std::swap(m_packet_queue, empty);
    }
    
    m_running = false;
    m_got_error = false;
    m_got_eos = false;
    m_capture_plane_started = false;
    
    return 0;
}

} // namespace bsp_codec
