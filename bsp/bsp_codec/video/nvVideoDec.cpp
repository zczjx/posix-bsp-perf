#include "nvVideoDec.hpp"
#include <bsp_g2d/BytesPerPixel.hpp>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <linux/videodev2.h>
#include <NvBufSurface.h>

#define MAX_PLANES 4
#define CHUNK_SIZE 4000000

namespace bsp_codec
{

nvVideoDec::nvVideoDec()
{
    std::cout << "nvVideoDec::nvVideoDec() constructor" << std::endl;
}

nvVideoDec::~nvVideoDec()
{
    std::cout << "nvVideoDec::~nvVideoDec() destructor" << std::endl;
    
    // Stop threads
    m_running = false;
    m_got_eos = true;
    
    // Wake up any waiting threads
    m_queue_cv.notify_all();
    
    // Join threads
    if (m_capture_thread && m_capture_thread->joinable())
    {
        m_capture_thread->join();
    }
    
    if (m_decoder_thread && m_decoder_thread->joinable())
    {
        m_decoder_thread->join();
    }
    
    // Cleanup DMA buffers
    for (int i = 0; i < m_num_capture_buffers; i++)
    {
        if (m_dmabuf_fds[i] != 0)
        {
            NvBufSurf::NvDestroy(m_dmabuf_fds[i]);
            m_dmabuf_fds[i] = 0;
        }
    }
    
    if (m_dst_dma_fd != -1)
    {
        NvBufSurf::NvDestroy(m_dst_dma_fd);
        m_dst_dma_fd = -1;
    }
    
    // Delete decoder
    if (m_dec != nullptr)
    {
        delete m_dec;
        m_dec = nullptr;
    }
}

uint32_t nvVideoDec::getV4L2PixelFormat(const std::string& encoding)
{
    if (encoding == "h264")
        return V4L2_PIX_FMT_H264;
    else if (encoding == "h265" || encoding == "hevc")
        return V4L2_PIX_FMT_H265;
    else if (encoding == "vp8")
        return V4L2_PIX_FMT_VP8;
    else if (encoding == "vp9")
        return V4L2_PIX_FMT_VP9;
    else if (encoding == "mpeg2")
        return V4L2_PIX_FMT_MPEG2;
    else if (encoding == "mpeg4")
        return V4L2_PIX_FMT_MPEG4;
    else if (encoding == "av1")
        return V4L2_PIX_FMT_AV1;
    else if (encoding == "mjpeg")
        return V4L2_PIX_FMT_MJPEG;
    
    std::cerr << "Unsupported encoding format: " << encoding << std::endl;
    return 0;
}

int nvVideoDec::setup(DecodeConfig& cfg)
{
    int ret = 0;
    
    std::cout << "nvVideoDec::setup() encoding=" << cfg.encoding << " fps=" << cfg.fps << std::endl;
    
    // Get V4L2 pixel format
    m_params.decoder_pixfmt = getV4L2PixelFormat(cfg.encoding);
    if (m_params.decoder_pixfmt == 0)
    {
        std::cerr << "Invalid encoding format" << std::endl;
        return -1;
    }
    
    m_params.fps = cfg.fps > 0 ? cfg.fps : 30;
    
    // Create decoder in blocking mode
    std::cout << "Creating NvVideoDecoder in blocking mode" << std::endl;
    m_dec = NvVideoDecoder::createVideoDecoder("dec0");
    if (!m_dec)
    {
        std::cerr << "Could not create NvVideoDecoder" << std::endl;
        return -1;
    }
    
    // Subscribe to resolution change event
    ret = m_dec->subscribeEvent(V4L2_EVENT_RESOLUTION_CHANGE, 0, 0);
    if (ret < 0)
    {
        std::cerr << "Could not subscribe to V4L2_EVENT_RESOLUTION_CHANGE" << std::endl;
        return -1;
    }
    
    // Set output plane format (encoded data)
    ret = m_dec->setOutputPlaneFormat(m_params.decoder_pixfmt, CHUNK_SIZE);
    if (ret < 0)
    {
        std::cerr << "Could not set output plane format" << std::endl;
        return -1;
    }
    
    // Set frame input mode (NAL units or chunks)
    if (m_params.input_nalu)
    {
        std::cout << "Setting frame input mode to NAL unit mode" << std::endl;
        ret = m_dec->setFrameInputMode(1);
        if (ret < 0)
        {
            std::cerr << "Error in decoder setFrameInputMode" << std::endl;
            return -1;
        }
    }
    
    // Setup output plane buffers (for encoded input)
    ret = m_dec->output_plane.setupPlane(V4L2_MEMORY_MMAP, m_params.num_output_buffers, true, false);
    if (ret < 0)
    {
        std::cerr << "Error while setting up output plane" << std::endl;
        return -1;
    }
    
    // Start stream on output plane
    ret = m_dec->output_plane.setStreamStatus(true);
    if (ret < 0)
    {
        std::cerr << "Error in output plane stream on" << std::endl;
        return -1;
    }
    
    // Start threads
    m_running = true;
    m_capture_thread = std::make_unique<std::thread>(&nvVideoDec::captureThreadFunc, this);
    m_decoder_thread = std::make_unique<std::thread>(&nvVideoDec::decoderThreadFunc, this);
    
    std::cout << "nvVideoDec::setup() completed successfully" << std::endl;
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
    if (ret < 0)
    {
        std::cerr << "Error: Could not get format from decoder capture plane" << std::endl;
        return -1;
    }
    
    // Get the display resolution from decoder
    ret = m_dec->capture_plane.getCrop(crop);
    if (ret < 0)
    {
        std::cerr << "Error: Could not get crop from decoder capture plane" << std::endl;
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
        ret = NvBufSurf::NvDestroy(m_dst_dma_fd);
        m_dst_dma_fd = -1;
        if (ret < 0)
        {
            std::cerr << "Error: Error in BufferDestroy" << std::endl;
        }
    }
    
    // Create PitchLinear output buffer for transform
    NvBufSurf::NvCommonAllocateParams params;
    params.memType = NVBUF_MEM_SURFACE_ARRAY;
    params.width = crop.c.width;
    params.height = crop.c.height;
    params.layout = NVBUF_LAYOUT_PITCH;
    params.colorFormat = NVBUF_COLOR_FORMAT_NV12;
    params.memtag = NvBufSurfaceTag_VIDEO_CONVERT;
    
    ret = NvBufSurf::NvAllocate(&params, 1, &m_dst_dma_fd);
    if (ret == -1)
    {
        std::cerr << "create transform dmabuf failed" << std::endl;
        return -1;
    }
    
    // Deinitialize capture plane
    m_dec->capture_plane.deinitPlane();
    
    // Destroy old capture buffers
    for (int i = 0; i < m_num_capture_buffers; i++)
    {
        if (m_dmabuf_fds[i] != 0)
        {
            ret = NvBufSurf::NvDestroy(m_dmabuf_fds[i]);
            if (ret < 0)
            {
                std::cerr << "Error: Error in BufferDestroy" << std::endl;
            }
            m_dmabuf_fds[i] = 0;
        }
    }
    
    // Set capture plane format
    ret = m_dec->setCapturePlaneFormat(format.fmt.pix_mp.pixelformat,
                                       format.fmt.pix_mp.width,
                                       format.fmt.pix_mp.height);
    if (ret < 0)
    {
        std::cerr << "Error in setting decoder capture plane format" << std::endl;
        return -1;
    }
    
    // Get minimum capture plane buffers
    ret = m_dec->getMinimumCapturePlaneBuffers(min_dec_capture_buffers);
    if (ret < 0)
    {
        std::cerr << "Error while getting value of minimum capture plane buffers" << std::endl;
        return -1;
    }
    
    m_num_capture_buffers = min_dec_capture_buffers + m_params.extra_cap_plane_buffer;
    
    // Allocate DMA buffers for capture plane
    NvBufSurf::NvCommonAllocateParams capParams;
    capParams.memType = NVBUF_MEM_SURFACE_ARRAY;
    capParams.width = crop.c.width;
    capParams.height = crop.c.height;
    capParams.layout = NVBUF_LAYOUT_BLOCK_LINEAR;
    capParams.colorFormat = NVBUF_COLOR_FORMAT_NV12;
    capParams.memtag = NvBufSurfaceTag_VIDEO_DEC;
    
    ret = NvBufSurf::NvAllocate(&capParams, m_num_capture_buffers, m_dmabuf_fds);
    if (ret < 0)
    {
        std::cerr << "Failed to create capture buffers" << std::endl;
        return -1;
    }
    
    // Request buffers on decoder capture plane
    ret = m_dec->capture_plane.reqbufs(V4L2_MEMORY_DMABUF, m_num_capture_buffers);
    if (ret)
    {
        std::cerr << "Error in request buffers on capture plane" << std::endl;
        return -1;
    }
    
    // Start stream on capture plane
    ret = m_dec->capture_plane.setStreamStatus(true);
    if (ret < 0)
    {
        std::cerr << "Error in decoder capture plane streamon" << std::endl;
        return -1;
    }
    
    m_capture_plane_started = true;
    
    // Enqueue all empty capture plane buffers
    for (uint32_t i = 0; i < m_dec->capture_plane.getNumBuffers(); i++)
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];
        
        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));
        
        v4l2_buf.index = i;
        v4l2_buf.m.planes = planes;
        v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        v4l2_buf.memory = V4L2_MEMORY_DMABUF;
        v4l2_buf.m.planes[0].m.fd = m_dmabuf_fds[i];
        
        ret = m_dec->capture_plane.qBuffer(v4l2_buf, NULL);
        if (ret < 0)
        {
            std::cerr << "Error Qing buffer at capture plane" << std::endl;
            return -1;
        }
    }
    
    std::cout << "Query and set capture successful" << std::endl;
    return 0;
}

void nvVideoDec::captureThreadFunc()
{
    std::cout << "nvVideoDec::captureThreadFunc() started" << std::endl;
    
    struct v4l2_event ev;
    int ret;
    
    // Wait for first resolution change event
    do
    {
        ret = m_dec->dqEvent(ev, 50000);
        if (ret < 0)
        {
            if (errno == EAGAIN)
            {
                std::cerr << "Timed out waiting for first V4L2_EVENT_RESOLUTION_CHANGE" << std::endl;
            }
            else
            {
                std::cerr << "Error in dequeueing decoder event" << std::endl;
            }
            m_got_error = true;
            return;
        }
    }
    while ((ev.type != V4L2_EVENT_RESOLUTION_CHANGE) && !m_got_error);
    
    // Setup capture plane
    if (!m_got_error)
    {
        if (queryAndSetCapture() < 0)
        {
            m_got_error = true;
            return;
        }
    }
    
    // Main capture loop
    while (m_running && !m_got_error && !m_dec->isInError())
    {
        NvBuffer *dec_buffer;
        
        // Check for resolution change
        ret = m_dec->dqEvent(ev, false);
        if (ret == 0)
        {
            if (ev.type == V4L2_EVENT_RESOLUTION_CHANGE)
            {
                std::cout << "Got V4L2_EVENT_RESOLUTION_CHANGE" << std::endl;
                if (queryAndSetCapture() < 0)
                {
                    m_got_error = true;
                    break;
                }
                continue;
            }
        }
        
        // Dequeue filled buffer from capture plane
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];
        
        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));
        v4l2_buf.m.planes = planes;
        
        if (m_dec->capture_plane.dqBuffer(v4l2_buf, &dec_buffer, NULL, 0))
        {
            if (errno == EAGAIN)
            {
                if (v4l2_buf.flags & V4L2_BUF_FLAG_LAST)
                {
                    std::cout << "Got EoS at capture plane" << std::endl;
                    break;
                }
                usleep(1000);
                continue;
            }
            else
            {
                m_got_error = true;
                std::cerr << "Error while calling dequeue at capture plane" << std::endl;
                break;
            }
        }
        
        // Transform from BlockLinear to PitchLinear
        NvBufSurf::NvCommonTransformParams transform_params;
        transform_params.src_top = 0;
        transform_params.src_left = 0;
        transform_params.src_width = m_display_width;
        transform_params.src_height = m_display_height;
        transform_params.dst_top = 0;
        transform_params.dst_left = 0;
        transform_params.dst_width = m_display_width;
        transform_params.dst_height = m_display_height;
        transform_params.flag = NVBUFSURF_TRANSFORM_FILTER;
        transform_params.flip = NvBufSurfTransform_None;
        transform_params.filter = NvBufSurfTransformInter_Nearest;
        
        ret = NvBufSurf::NvTransform(&transform_params, m_dmabuf_fds[v4l2_buf.index], m_dst_dma_fd);
        if (ret == -1)
        {
            std::cerr << "Transform failed" << std::endl;
        }
        else if (m_callback != nullptr)
        {
            // Call user callback with decoded frame
            std::shared_ptr<DecodeOutFrame> frame = std::make_shared<DecodeOutFrame>();
            
            // Map the transformed buffer to get virtual address
            NvBufSurface *surf = nullptr;
            ret = NvBufSurfaceFromFd(m_dst_dma_fd, (void**)&surf);
            if (ret == 0 && surf != nullptr)
            {
                ret = NvBufSurfaceMap(surf, 0, 0, NVBUF_MAP_READ);
                if (ret == 0)
                {
                    frame->width = m_display_width;
                    frame->height = m_display_height;
                    frame->width_stride = surf->surfaceList[0].planeParams.pitch[0];
                    frame->height_stride = m_display_height;
                    frame->format = "YUV420SP";  // NV12
                    frame->virt_addr = (uint8_t*)surf->surfaceList[0].mappedAddr.addr[0];
                    frame->valid_data_size = frame->width * frame->height * 3 / 2;
                    frame->fd = m_dst_dma_fd;
                    frame->eos_flag = 0;
                    
                    m_callback(m_userdata, frame);
                    
                    NvBufSurfaceUnMap(surf, 0, 0);
                }
            }
        }
        
        // Queue buffer back to capture plane
        v4l2_buf.m.planes[0].m.fd = m_dmabuf_fds[v4l2_buf.index];
        if (m_dec->capture_plane.qBuffer(v4l2_buf, NULL) < 0)
        {
            m_got_error = true;
            std::cerr << "Error while queueing buffer at decoder capture plane" << std::endl;
            break;
        }
    }
    
    std::cout << "nvVideoDec::captureThreadFunc() exiting" << std::endl;
}

void nvVideoDec::decoderThreadFunc()
{
    std::cout << "nvVideoDec::decoderThreadFunc() started" << std::endl;
    
    // Wait for packets in queue
    while (m_running && !m_got_error && !m_dec->isInError())
    {
        PacketInfo pkt;
        
        {
            std::unique_lock<std::mutex> lock(m_queue_mutex);
            m_queue_cv.wait(lock, [this] { 
                return !m_packet_queue.empty() || !m_running || m_got_eos; 
            });
            
            if (!m_running || m_got_eos)
                break;
                
            if (m_packet_queue.empty())
                continue;
                
            pkt = std::move(m_packet_queue.front());
            m_packet_queue.pop();
        }
        
        // Process the packet - queue to decoder
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];
        NvBuffer *buffer;
        
        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));
        v4l2_buf.m.planes = planes;
        
        // Dequeue buffer from output plane
        int ret = m_dec->output_plane.dqBuffer(v4l2_buf, &buffer, NULL, -1);
        if (ret < 0)
        {
            std::cerr << "Error DQing buffer at output plane" << std::endl;
            m_got_error = true;
            break;
        }
        
        // Copy packet data to buffer
        if (!pkt.is_eos && pkt.data.size() > 0)
        {
            size_t bytes_to_copy = std::min(pkt.data.size(), (size_t)buffer->planes[0].length);
            memcpy(buffer->planes[0].data, pkt.data.data(), bytes_to_copy);
            buffer->planes[0].bytesused = bytes_to_copy;
        }
        else
        {
            buffer->planes[0].bytesused = 0;  // EOS marker
        }
        
        v4l2_buf.m.planes[0].bytesused = buffer->planes[0].bytesused;
        
        // Queue buffer back to output plane
        ret = m_dec->output_plane.qBuffer(v4l2_buf, NULL);
        if (ret < 0)
        {
            std::cerr << "Error Qing buffer at output plane" << std::endl;
            m_got_error = true;
            break;
        }
        
        if (pkt.is_eos)
        {
            std::cout << "Sent EOS to decoder" << std::endl;
            break;
        }
    }
    
    std::cout << "nvVideoDec::decoderThreadFunc() exiting" << std::endl;
}

int nvVideoDec::decode(DecodePacket& pkt_data)
{
    if (!m_running || m_got_error)
    {
        std::cerr << "Decoder not running or in error state" << std::endl;
        return -1;
    }
    
    // Add packet to queue
    PacketInfo pkt;
    if (pkt_data.pkt_eos)
    {
        pkt.is_eos = true;
        m_got_eos = true;
    }
    else if (pkt_data.data != nullptr && pkt_data.pkt_size > 0)
    {
        pkt.data.assign(pkt_data.data, pkt_data.data + pkt_data.pkt_size);
        pkt.is_eos = false;
    }
    else
    {
        return 0;  // Empty packet, ignore
    }
    
    {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        m_packet_queue.push(std::move(pkt));
    }
    m_queue_cv.notify_one();
    
    return 0;
}

int nvVideoDec::reset()
{
    std::cout << "nvVideoDec::reset()" << std::endl;
    
    // Clear packet queue
    {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        while (!m_packet_queue.empty())
        {
            m_packet_queue.pop();
        }
    }
    
    // Reset decoder state
    m_got_eos = false;
    m_got_error = false;
    
    return 0;
}

} // namespace bsp_codec

