#include "rkmppCodecHeader.hpp"
#include "rkmppDec.hpp"
#include <bsp_g2d/BytesPerPixel.hpp>
#include <iostream>
#include <sys/time.h>
#include <unistd.h>
namespace bsp_codec
{

static inline unsigned long GetCurrentTimeMS()
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return tv.tv_sec*1000 + tv.tv_usec / 1000;
}

rkmppDec::~rkmppDec()
{
    if (m_ctx.packet != nullptr)
    {
        mpp_packet_deinit(&m_ctx.packet);
        m_ctx.packet = nullptr;
    }
    if (m_ctx.frame != nullptr)
    {
        mpp_frame_deinit(&m_ctx.frame);
        m_ctx.frame = nullptr;
    }
    if (m_ctx.mpp_ctx != nullptr)
    {
        mpp_destroy(m_ctx.mpp_ctx);
        m_ctx.mpp_ctx = nullptr;
    }
    if (m_ctx.frm_grp != nullptr)
    {
        mpp_buffer_group_put(m_ctx.frm_grp);
        m_ctx.frm_grp = nullptr;
    }
}

int rkmppDec::setup(DecodeConfig& cfg)
{
    int ret = 0;
    try
    {
        m_params.encoding_type = rkmppCodecHeader::getInstance().strToMppCoding(cfg.encoding);
        std::cout << "rkmpp dec m_params.encoding_type= " << m_params.encoding_type << std::endl;
    }
    catch (const std::out_of_range& e)
    {
        std::cerr << "Invalid encoding type " << cfg.encoding << std::endl;
        return -1;
    }
    m_params.fps = cfg.fps;

    std::cout << "rkmppDec::setup() mpi_dec_test start " << std::endl;

    ret = mpp_create(&m_ctx.mpp_ctx, &m_ctx.mpi);

    if (MPP_OK != ret)
    {
        std::cerr << "mpp_create failed" << std::endl;
        return -1;
    }

    ret = mpp_init(m_ctx.mpp_ctx, MPP_CTX_DEC, m_params.encoding_type);

    if (MPP_OK != ret)
    {
        std::cerr << "mpp_init failed" << std::endl;
        return -1;
    }

    MppDecCfg mpp_dec_cfg{nullptr};
    mpp_dec_cfg_init(&mpp_dec_cfg);

    /* get default config from decoder context */
    ret = m_ctx.mpi->control(m_ctx.mpp_ctx, MPP_DEC_GET_CFG, mpp_dec_cfg);
    if (MPP_OK != ret)
    {
        std::cerr << "failed to get decoder cfg" << std::endl;
        return -1;
    }

    /*
     * split_parse is to enable mpp internal frame spliter when the input
     * packet is not aplited into frames.
     */
    ret = mpp_dec_cfg_set_u32(mpp_dec_cfg, "base:split_parse", m_params.need_split);

    if (MPP_OK != ret)
    {
        std::cerr << "failed to set split_parse ret: " << ret << std::endl;
        return -1;
    }

    ret = m_ctx.mpi->control(m_ctx.mpp_ctx, MPP_DEC_SET_CFG, mpp_dec_cfg);

    if (MPP_OK != ret)
    {
        std::cerr << "failed to set decoder cfg ret: " << ret << std::endl;
        return -1;
    }

    mpp_dec_cfg_deinit(mpp_dec_cfg);
    return 0;
}

int rkmppDec::decode(DecodePacket& pkt_data)
{
    RK_U32 err_info = 0;
    MPP_RET ret = MPP_OK;
    size_t read_size = 0;

    std::cout << "rkmppDec::decode() receive packet size= " << pkt_data.pkt_size << std::endl;

    if (m_packet == nullptr)
    {
        ret = mpp_packet_init(&m_packet, nullptr, 0);
    }

    ///////////////////////////////////////////////
    mpp_packet_set_data(m_packet, pkt_data.data);
    mpp_packet_set_size(m_packet, pkt_data.pkt_size);
    mpp_packet_set_pos(m_packet, pkt_data.data);
    mpp_packet_set_length(m_packet, pkt_data.pkt_size);
    // setup eos flag
    if (pkt_data.pkt_eos)
    {
        mpp_packet_set_eos(m_packet);
    }

    RK_U32 pkt_done = 0;
    do
    {

        RK_S32 times = 5;
        // send the packet first if packet is not done
        if (!pkt_done)
        {
            ret = m_ctx.mpi->decode_put_packet(m_ctx.mpp_ctx, m_packet);

            if (MPP_OK == ret)
            {
                pkt_done = 1;
            }
        }
        // then get all available frame and release
        do
        {
            RK_S32 get_frm = 0;
            RK_U32 frm_eos = 0;

        try_again:
            ret = m_ctx.mpi->decode_get_frame(m_ctx.mpp_ctx, &m_frame);

            if (MPP_ERR_TIMEOUT == ret)
            {
                if (times > 0)
                {
                    times--;
                    usleep(2000);
                    goto try_again;
                }
                std::cerr << "decode_get_frame failed too much time" << std::endl;
            }

            if (MPP_OK != ret)
            {
                std::cerr << "decode_get_frame failed ret " << ret << std::endl;
                break;
            }

            if (m_frame != nullptr)
            {
                RK_U32 hor_stride = mpp_frame_get_hor_stride(m_frame);
                RK_U32 ver_stride = mpp_frame_get_ver_stride(m_frame);
                RK_U32 hor_width = mpp_frame_get_width(m_frame);
                RK_U32 ver_height = mpp_frame_get_height(m_frame);
                RK_U32 buf_size = mpp_frame_get_buf_size(m_frame);
                RK_S64 pts = mpp_frame_get_pts(m_frame);
                RK_S64 dts = mpp_frame_get_dts(m_frame);

                std::cout << "decoder require buffer w:h [" << hor_width << ":" << ver_height
                << "] stride [" << hor_stride << ":" << ver_stride << "] buf_size " << buf_size
                << " pts=" << pts << " dts=" << dts << std::endl;

                if (mpp_frame_get_info_change(m_frame))
                {
                    std::cout << "decode_get_frame get info changed found " << std::endl;

                    if (nullptr == m_ctx.frm_grp)
                    {
                        /* If buffer group is not set create one and limit it */
                        ret = mpp_buffer_group_get_internal(&m_ctx.frm_grp, MPP_BUFFER_TYPE_DRM);

                        if (ret)
                        {
                            std::cerr << "get mpp buffer group failed ret " << ret << std::endl;
                            break;
                        }

                        /* Set buffer to mpp decoder */
                        ret = m_ctx.mpi->control(m_ctx.mpp_ctx, MPP_DEC_SET_EXT_BUF_GROUP, m_ctx.frm_grp);

                        if (ret)
                        {
                            std::cerr << "set buffer group failed ret " << ret << std::endl;
                            break;
                        }
                    }
                    else
                    {
                        /* If old buffer group exist clear it */
                        ret = mpp_buffer_group_clear(m_ctx.frm_grp);

                        if (ret)
                        {
                            std::cerr << "clear buffer group failed ret " << ret << std::endl;
                            break;
                        }
                    }

                    /* Use limit config to limit buffer count to 24 with buf_size */
                    ret = mpp_buffer_group_limit_config(m_ctx.frm_grp, buf_size, 24);

                    if (ret)
                    {
                        std::cerr << "limit buffer group failed ret " << ret << std::endl;
                        break;
                    }

                    /*
                     * All buffer group config done. Set info change ready to let
                     * decoder continue decoding
                     */
                    ret = m_ctx.mpi->control(m_ctx.mpp_ctx, MPP_DEC_SET_INFO_CHANGE_READY, nullptr);

                    if (ret)
                    {
                        std::cerr << "info change ready failed ret " << ret << std::endl;
                        break;
                    }

                    m_params.last_frame_time_ms = GetCurrentTimeMS();
                }
                else
                {
                    err_info = mpp_frame_get_errinfo(m_frame) | mpp_frame_get_discard(m_frame);

                    if (err_info)
                    {
                        std::cerr << "decoder_get_frame get err info: " << err_info << " discard: " << mpp_frame_get_discard(m_frame) << std::endl;
                    }
                    m_ctx.frame_count++;
                    struct timeval tv;
                    gettimeofday(&tv, NULL);
                    std::cout << "get one frame " << (tv.tv_sec * 1000 + tv.tv_usec / 1000) << std::endl;

                    if (m_callback != nullptr)
                    {
                        MppFrameFormat format = mpp_frame_get_fmt(m_frame);
                        uint8_t *data_vir =(uint8_t *) mpp_buffer_get_ptr(mpp_frame_get_buffer(m_frame));
                        int fd = mpp_buffer_get_fd(mpp_frame_get_buffer(m_frame));
                        std::shared_ptr<DecodeOutFrame> frame = std::make_shared<DecodeOutFrame>();
                        frame->width = hor_width;
                        frame->height = ver_height;
                        frame->width_stride = hor_stride;
                        frame->height_stride = ver_stride;
                        frame->format = rkmppCodecHeader::getInstance().mppFrameFormatToStr(format);
                        frame->virt_addr = data_vir;
                        frame->valid_data_size = frame->width * frame->height * bsp_g2d::BytesPerPixel::getInstance().getBytesPerPixel(frame->format);
                        frame->fd = fd;
                        m_callback(m_userdata, frame);
                    }
                    unsigned long cur_time_ms = GetCurrentTimeMS();
                    long time_gap = (1000 / m_params.fps) - (cur_time_ms - m_params.last_frame_time_ms);
                    std::cout << "time_gap=" << time_gap << std::endl;

                    if (time_gap > 0)
                    {
                        usleep(time_gap * 1000);
                    }

                    m_params.last_frame_time_ms = GetCurrentTimeMS();
                }

                frm_eos = mpp_frame_get_eos(m_frame);
                ret = mpp_frame_deinit(&m_frame);
                m_frame = nullptr;
                get_frm = 1;
            }

            // try get runtime frame memory usage
            if (m_ctx.frm_grp)
            {
                size_t usage = mpp_buffer_group_usage(m_ctx.frm_grp);
                if (usage > m_ctx.max_usage)
                {
                    m_ctx.max_usage = usage;
                }
            }

            // if last packet is send but last frame is not found continue
            if (pkt_data.pkt_eos && pkt_done && !frm_eos)
            {
                usleep(1*1000);
                continue;
            }

            if (frm_eos)
            {
                std::cout << "found last frame " << std::endl;
                break;
            }

            if (m_ctx.frame_num > 0 && m_ctx.frame_count >= m_ctx.frame_num)
            {
                m_ctx.eos = 1;
                break;
            }

            if (get_frm)
            {
                continue;
            }

            break;
        }
        while (1);

        if (m_ctx.frame_num > 0 && m_ctx.frame_count >= m_ctx.frame_num)
        {
            m_ctx.eos = 1;
            std::cout << "reach max frame number " << m_ctx.frame_count << std::endl;
            break;
        }

        if (pkt_done)
        {
            break;
        }

        /*
         * why sleep here:
         * mpi->decode_put_packet will failed when packet in internal queue is
         * full,waiting the package is consumed .Usually hardware decode one
         * frame which resolution is 1080p needs 2 ms,so here we sleep 3ms
         * * is enough.
         */
        usleep(3*1000);
    }
    while (1);

    mpp_packet_deinit(&m_packet);

    return ret;
}

int rkmppDec::tearDown()
{
    if (m_ctx.mpi != nullptr)
    {
        m_ctx.mpi->reset(m_ctx.mpp_ctx);
    }
    return 0;
}

} // namespace bsp_codec