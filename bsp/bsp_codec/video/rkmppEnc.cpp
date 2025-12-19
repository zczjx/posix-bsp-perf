#include "rkmppCodecHeader.hpp"
#include "rkmppEnc.hpp"
#include <rockchip/rk_venc_rc.h>
#include <iostream>
#include <cstring>
#include <sys/time.h>
#include <unistd.h>

namespace bsp_codec
{
rkmppEnc::~rkmppEnc()
{
    if (m_ctx.mpp_ctx != nullptr)
    {
        mpp_destroy(m_ctx.mpp_ctx);
        m_ctx.mpp_ctx = nullptr;
    }

    if (m_ctx.frm_buf != nullptr)
    {
        mpp_buffer_put(m_ctx.frm_buf);
        m_ctx.frm_buf = nullptr;
    }

    if (m_ctx.pkt_buf)
    {
        mpp_buffer_put(m_ctx.pkt_buf);
        m_ctx.pkt_buf = nullptr;
    }

    if (m_ctx.md_info)
    {
        mpp_buffer_put(m_ctx.md_info);
        m_ctx.md_info = nullptr;
    }

    if (m_ctx.buf_grp)
    {
        mpp_buffer_group_put(m_ctx.buf_grp);
        m_ctx.buf_grp = nullptr;
    }

}

constexpr auto MPP_ALIGN = [](auto x, auto a) { return ((x + a - 1) & ~(a - 1)); };

int rkmppEnc::calculateFrameSize(MppFrameFormat frameFormat)
{
    int frame_size = 0;

    switch (frameFormat & MPP_FRAME_FMT_MASK)
    {
        case MPP_FMT_YUV420SP:
        case MPP_FMT_YUV420P:
            frame_size = MPP_ALIGN(m_params.hor_stride, 64) * MPP_ALIGN(m_params.ver_stride, 64) * 3 / 2;
            break;

        case MPP_FMT_YUV422_YUYV :
        case MPP_FMT_YUV422_YVYU :
        case MPP_FMT_YUV422_UYVY :
        case MPP_FMT_YUV422_VYUY :
        case MPP_FMT_YUV422P :
        case MPP_FMT_YUV422SP :
            frame_size = MPP_ALIGN(m_params.hor_stride, 64) * MPP_ALIGN(m_params.ver_stride, 64) * 2;
            break;

        case MPP_FMT_RGB444 :
        case MPP_FMT_BGR444 :
        case MPP_FMT_RGB555 :
        case MPP_FMT_BGR555 :
        case MPP_FMT_RGB565 :
        case MPP_FMT_BGR565 :
        case MPP_FMT_RGB888 :
        case MPP_FMT_BGR888 :
        case MPP_FMT_RGB101010 :
        case MPP_FMT_BGR101010 :
        case MPP_FMT_ARGB8888 :
        case MPP_FMT_ABGR8888 :
        case MPP_FMT_BGRA8888 :
        case MPP_FMT_RGBA8888 :
            frame_size = MPP_ALIGN(m_params.hor_stride, 64) * MPP_ALIGN(m_params.ver_stride, 64);
            break;

        default:
            frame_size = MPP_ALIGN(m_params.hor_stride, 64) * MPP_ALIGN(m_params.ver_stride, 64) * 4;
            break;
    }
    return frame_size;

}

int rkmppEnc::parseConfig(EncodeConfig& cfg)
{
    m_params.width = cfg.width;
    m_params.height = cfg.height;
    m_params.hor_stride = cfg.hor_stride;
    m_params.ver_stride = cfg.ver_stride;
    m_params.frameFormat = rkmppCodecHeader::getInstance().strToMppFrameFormat(cfg.frameFormat);

    // get paramter from cmd
    if (m_params.hor_stride == 0)
    {
        m_params.hor_stride = MPP_ALIGN(m_params.width, 16);
    }
    if (m_params.ver_stride == 0)
    {
        m_params.ver_stride = (MPP_ALIGN(m_params.height, 16));
    }

    if (m_params.bps == 0)
    {
        m_params.bps = m_params.width * m_params.height / 8 * (m_params.fps_out_num / m_params.fps_out_den);
    }
    m_ctx.mdinfo_size = (MPP_VIDEO_CodingHEVC == m_params.encoding_type) ?
                      (MPP_ALIGN(m_params.hor_stride, 32) >> 5) *
                      (MPP_ALIGN(m_params.ver_stride, 32) >> 5) * 16 :
                      (MPP_ALIGN(m_params.hor_stride, 64) >> 6) *
                      (MPP_ALIGN(m_params.ver_stride, 16) >> 4) * 16;

    m_ctx.frame_size = calculateFrameSize(m_params.frameFormat);
    return 0;
}

int rkmppEnc::setupMppEncCfg()
{
    MPP_RET ret;
    MppEncCfg cfg{nullptr};

    ret = mpp_enc_cfg_init(&cfg);
    if (ret != MPP_OK)
    {
        std::cerr << "mpp_enc_cfg_init failed ret: " << ret << std::endl;
        return -1;
    }

    mpp_enc_cfg_set_s32(cfg, "prep:width", m_params.width);
    mpp_enc_cfg_set_s32(cfg, "prep:height", m_params.height);
    mpp_enc_cfg_set_s32(cfg, "prep:hor_stride", m_params.hor_stride);
    mpp_enc_cfg_set_s32(cfg, "prep:ver_stride", m_params.ver_stride);
    mpp_enc_cfg_set_s32(cfg, "prep:format", m_params.frameFormat);

    mpp_enc_cfg_set_s32(cfg, "rc:mode", m_params.rc_mode);

    /* fix input / output frame rate */
    mpp_enc_cfg_set_s32(cfg, "rc:fps_in_flex", m_params.fps_in_flex);
    mpp_enc_cfg_set_s32(cfg, "rc:fps_in_num", m_params.fps_in_num);
    mpp_enc_cfg_set_s32(cfg, "rc:fps_in_denorm", m_params.fps_in_den);
    mpp_enc_cfg_set_s32(cfg, "rc:fps_out_flex", m_params.fps_out_flex);
    mpp_enc_cfg_set_s32(cfg, "rc:fps_out_num", m_params.fps_out_num);
    mpp_enc_cfg_set_s32(cfg, "rc:fps_out_denorm", m_params.fps_out_den);
    mpp_enc_cfg_set_s32(cfg, "rc:gop", m_params.gop_len ? m_params.gop_len : m_params.fps_out_num * 2);

    /* drop frame or not when bitrate overflow */
    mpp_enc_cfg_set_u32(cfg, "rc:drop_mode", MPP_ENC_RC_DROP_FRM_DISABLED);
    mpp_enc_cfg_set_u32(cfg, "rc:drop_thd", 20);        /* 20% of max bps */
    mpp_enc_cfg_set_u32(cfg, "rc:drop_gap", 1);         /* Do not continuous drop frame */

    /* setup bitrate for different rc_mode */
    mpp_enc_cfg_set_s32(cfg, "rc:bps_target", m_params.bps);
    switch (m_params.rc_mode)
    {
        case MPP_ENC_RC_MODE_FIXQP:
            /* do not setup bitrate on FIXQP mode */
            break;
        case MPP_ENC_RC_MODE_CBR:
            {
                /* CBR mode has narrow bound */
                mpp_enc_cfg_set_s32(cfg, "rc:bps_max", m_params.bps_max ? m_params.bps_max : m_params.bps * 17 / 16);
                mpp_enc_cfg_set_s32(cfg, "rc:bps_min", m_params.bps_min ? m_params.bps_min : m_params.bps * 15 / 16);
            }
            break;
        case MPP_ENC_RC_MODE_VBR:
        case MPP_ENC_RC_MODE_AVBR:
            {
                /* VBR mode has wide bound */
                mpp_enc_cfg_set_s32(cfg, "rc:bps_max", m_params.bps_max ? m_params.bps_max : m_params.bps * 17 / 16);
                mpp_enc_cfg_set_s32(cfg, "rc:bps_min", m_params.bps_min ? m_params.bps_min : m_params.bps * 1 / 16);
            }
            break;
        default:
            {
                /* default use CBR mode */
                mpp_enc_cfg_set_s32(cfg, "rc:bps_max", m_params.bps_max ? m_params.bps_max : m_params.bps * 17 / 16);
                mpp_enc_cfg_set_s32(cfg, "rc:bps_min", m_params.bps_min ? m_params.bps_min : m_params.bps * 15 / 16);
            }
            break;
    }
    /* setup qp for different codec and rc_mode */
    switch (m_params.encoding_type)
    {
        case MPP_VIDEO_CodingAVC:
        case MPP_VIDEO_CodingHEVC:
            switch (m_params.rc_mode)
            {
                case MPP_ENC_RC_MODE_FIXQP:
                {
                    RK_S32 fix_qp = m_params.qp_init;
                    mpp_enc_cfg_set_s32(cfg, "rc:qp_init", fix_qp);
                    mpp_enc_cfg_set_s32(cfg, "rc:qp_max", fix_qp);
                    mpp_enc_cfg_set_s32(cfg, "rc:qp_min", fix_qp);
                    mpp_enc_cfg_set_s32(cfg, "rc:qp_max_i", fix_qp);
                    mpp_enc_cfg_set_s32(cfg, "rc:qp_min_i", fix_qp);
                    mpp_enc_cfg_set_s32(cfg, "rc:qp_ip", 0);
                }
                break;
                case MPP_ENC_RC_MODE_CBR:
                case MPP_ENC_RC_MODE_VBR:
                case MPP_ENC_RC_MODE_AVBR:
                {
                    mpp_enc_cfg_set_s32(cfg, "rc:qp_init", -1);
                    mpp_enc_cfg_set_s32(cfg, "rc:qp_max", 51);
                    mpp_enc_cfg_set_s32(cfg, "rc:qp_min", 10);
                    mpp_enc_cfg_set_s32(cfg, "rc:qp_max_i", 51);
                    mpp_enc_cfg_set_s32(cfg, "rc:qp_min_i", 10);
                    mpp_enc_cfg_set_s32(cfg, "rc:qp_ip", 2);
                }
                break;
                default:
                    std::cerr << "unsupport encoder rc mode " << m_params.rc_mode << std::endl;
                    break;
            }
            break;
        case MPP_VIDEO_CodingVP8:
            /* vp8 only setup base qp range */
            mpp_enc_cfg_set_s32(cfg, "rc:qp_init", 40);
            mpp_enc_cfg_set_s32(cfg, "rc:qp_max",  127);
            mpp_enc_cfg_set_s32(cfg, "rc:qp_min",  0);
            mpp_enc_cfg_set_s32(cfg, "rc:qp_max_i", 127);
            mpp_enc_cfg_set_s32(cfg, "rc:qp_min_i", 0);
            mpp_enc_cfg_set_s32(cfg, "rc:qp_ip", 6);
            break;
        case MPP_VIDEO_CodingMJPEG:
            /* jpeg use special codec config to control qtable */
            mpp_enc_cfg_set_s32(cfg, "jpeg:q_factor", 80);
            mpp_enc_cfg_set_s32(cfg, "jpeg:qf_max", 99);
            mpp_enc_cfg_set_s32(cfg, "jpeg:qf_min", 1);
            break;
        default:
            break;
    }

    /* setup codec  */
    mpp_enc_cfg_set_s32(cfg, "codec:type", m_params.encoding_type);
    switch (m_params.encoding_type)
    {
        case MPP_VIDEO_CodingAVC:
        {
            RK_U32 constraint_set = m_params.constraint_set;

            /*
            * H.264 profile_idc parameter
            * 66  - Baseline profile
            * 77  - Main profile
            * 100 - High profile
            */
            mpp_enc_cfg_set_s32(cfg, "h264:profile", 100);
            /*
            * H.264 level_idc parameter
            * 10 / 11 / 12 / 13    - qcif@15fps / cif@7.5fps / cif@15fps / cif@30fps
            * 20 / 21 / 22         - cif@30fps / half-D1@@25fps / D1@12.5fps
            * 30 / 31 / 32         - D1@25fps / 720p@30fps / 720p@60fps
            * 40 / 41 / 42         - 1080p@30fps / 1080p@30fps / 1080p@60fps
            * 50 / 51 / 52         - 4K@30fps
            */
            mpp_enc_cfg_set_s32(cfg, "h264:level", 40);
            mpp_enc_cfg_set_s32(cfg, "h264:cabac_en", 1);
            mpp_enc_cfg_set_s32(cfg, "h264:cabac_idc", 0);
            mpp_enc_cfg_set_s32(cfg, "h264:trans8x8", 1);

            if (constraint_set & 0x3f0000)
                mpp_enc_cfg_set_s32(cfg, "h264:constraint_set", constraint_set);
        }
            break;
        case MPP_VIDEO_CodingHEVC:
        case MPP_VIDEO_CodingMJPEG:
        case MPP_VIDEO_CodingVP8:
            break;
        default:
        {
            std::cerr << "unsupport encoder coding type " << m_params.encoding_type << std::endl;
            break;
        }
    }

    if (m_params.split_mode)
    {
        std::cout << "split mode " << m_params.split_mode << " arg " << m_params.split_arg << " out " << m_params.split_out << std::endl;
        mpp_enc_cfg_set_s32(cfg, "split:mode", m_params.split_mode);
        mpp_enc_cfg_set_s32(cfg, "split:arg", m_params.split_arg);
        mpp_enc_cfg_set_s32(cfg, "split:out", m_params.split_out);
    }

    mpp_enc_cfg_set_s32(cfg, "prep:mirroring", m_params.mirroring);
    mpp_enc_cfg_set_s32(cfg, "prep:rotation", m_params.rotation);
    mpp_enc_cfg_set_s32(cfg, "prep:flip", m_params.flip);

    ret = m_ctx.mpi->control(m_ctx.mpp_ctx, MPP_ENC_SET_CFG, cfg);

    if (ret != MPP_OK)
    {
        std::cerr << "mpi control enc set cfg failed ret: " << ret << std::endl;
        mpp_enc_cfg_deinit(cfg);
        return -1;
    }

    if (m_params.encoding_type == MPP_VIDEO_CodingAVC || m_params.encoding_type == MPP_VIDEO_CodingHEVC)
    {
        m_params.header_mode = MPP_ENC_HEADER_MODE_EACH_IDR;
        ret = m_ctx.mpi->control(m_ctx.mpp_ctx, MPP_ENC_SET_HEADER_MODE, &m_params.header_mode);
        if (ret != MPP_OK)
        {
            std::cerr << "mpi control enc set header mode failed ret: " << ret << std::endl;
            mpp_enc_cfg_deinit(cfg);
            return -1;
        }
    }

    return ret;
}

int rkmppEnc::setup(EncodeConfig& cfg)
{
    int ret;
    MppPollType timeout = MPP_POLL_BLOCK;

    parseConfig(cfg);

    ret = mpp_buffer_group_get_internal(&m_ctx.buf_grp, MPP_BUFFER_TYPE_DRM);

    if (ret != MPP_SUCCESS)
    {
        std::cerr << "Failed to get buffer group ret: " << ret << std::endl;
        return -1;
    }

    ret = mpp_buffer_get(m_ctx.buf_grp, &m_ctx.pkt_buf, m_ctx.frame_size);

    if (ret != MPP_SUCCESS)
    {
        std::cerr << "Failed to get buffer for output packet ret: " << ret << std::endl;
        return -1;
    }

    ret = mpp_buffer_get(m_ctx.buf_grp, &m_ctx.md_info, m_ctx.mdinfo_size);

    if (ret != MPP_SUCCESS)
    {
        std::cerr << "Failed to get buffer for motion info output packet ret: " << ret << std::endl;
        return -1;
    }

    // encoder demo
    ret = mpp_create(&m_ctx.mpp_ctx, &m_ctx.mpi);

    if (MPP_SUCCESS != ret)
    {
        std::cerr << "mpp_create failed" << std::endl;
        return -1;
    }

    std::cout << "mpp_ctx " << m_ctx.mpp_ctx << " mpi " << m_ctx.mpi << std::endl;
    std::cout << "encoder test start w " << m_params.width << " h " << m_params.height << " type " << m_params.encoding_type << std::endl;

    ret = m_ctx.mpi->control(m_ctx.mpp_ctx, MPP_SET_OUTPUT_TIMEOUT, &timeout);

    if (MPP_SUCCESS != ret)
    {
        std::cerr << "mpi control set output timeout " << timeout << " ret " << ret << std::endl;
        return -1;
    }

    ret = mpp_init(m_ctx.mpp_ctx, MPP_CTX_ENC, m_params.encoding_type);

    if (MPP_SUCCESS != ret)
    {
        std::cerr << "mpp_init failed ret: " << ret << std::endl;
        return -1;
    }

    return setupMppEncCfg();
}

std::shared_ptr<EncodeInputBuffer> rkmppEnc::getInputBuffer()
{
    if (m_ctx.frm_buf == nullptr)
    {
        int ret = mpp_buffer_get(m_ctx.buf_grp, &m_ctx.frm_buf, m_ctx.frame_size);
        if (ret != MPP_SUCCESS)
        {
            std::cerr << "Failed to get buffer for input frame ret: " << ret << std::endl;
            return nullptr;
        }
    }

    std::shared_ptr<EncodeInputBuffer> inputBuffer = std::make_shared<EncodeInputBuffer>();
    inputBuffer->internal_buf = m_ctx.frm_buf;
    inputBuffer->input_buf_fd = mpp_buffer_get_fd(m_ctx.frm_buf);
    inputBuffer->input_buf_addr = mpp_buffer_get_ptr(m_ctx.frm_buf);
    return inputBuffer;
}

int rkmppEnc::encode(EncodeInputBuffer& input_buf, EncodePacket& out_pkt)
{
    MPP_RET ret;
    RK_U32 frm_eos = 0;

    MppFrame frame{nullptr};
    ret = mpp_frame_init(&frame);
    if (ret != MPP_OK)
    {
        std::cerr << "mpp_frame_init failed" << std::endl;
        return -1;
    }

    mpp_frame_set_width(frame, m_params.width);
    mpp_frame_set_height(frame, m_params.height);
    mpp_frame_set_hor_stride(frame, m_params.hor_stride);
    mpp_frame_set_ver_stride(frame, m_params.ver_stride);
    mpp_frame_set_fmt(frame, m_params.frameFormat);
    mpp_frame_set_eos(frame, frm_eos);
    MppBuffer mpp_buf = std::any_cast<MppBuffer>(input_buf.internal_buf);
    mpp_frame_set_buffer(frame, mpp_buf);
    MppMeta meta = mpp_frame_get_meta(frame);
    MppPacket packet{nullptr};
    mpp_packet_init_with_buffer(&packet, m_ctx.pkt_buf);
    /* NOTE: It is important to clear output packet length!! */
    mpp_packet_set_length(packet, 0);
    mpp_meta_set_packet(meta, KEY_OUTPUT_PACKET, packet);
    mpp_meta_set_buffer(meta, KEY_MOTION_INFO, m_ctx.md_info);

    /*
    * NOTE: in non-block mode the frame can be resent.
    * The default input timeout mode is block.
    *
    * User should release the input frame to meet the requirements of
    * resource creator must be the resource destroyer.
    */
    ret = m_ctx.mpi->encode_put_frame(m_ctx.mpp_ctx, frame);
    mpp_frame_deinit(&frame);

    if (ret != MPP_OK)
    {
        std::cerr << "encode put frame failed ret: " << ret << std::endl;
        return -1;
    }

    size_t out_len = 0;
    RK_U32 eoi = 1;
    void* out_ptr = out_pkt.encode_pkt.data();
    do
    {
        ret = m_ctx.mpi->encode_get_packet(m_ctx.mpp_ctx, &packet);

        if (ret != MPP_OK)
        {
            std::cerr << "encode get packet failed ret: " << ret << std::endl;
            return -1;
        }
        if (packet != nullptr)
        {
            // write packet to file here
            void *ptr   = mpp_packet_get_pos(packet);
            size_t len  = mpp_packet_get_length(packet);
            char log_buf[256];
            RK_S32 log_size = sizeof(log_buf) - 1;
            RK_S32 log_len = 0;

            // if (!enc_params.first_pkt)
            //     enc_params.first_pkt = mpp_time();

            RK_U32 pkt_eos = mpp_packet_get_eos(packet);

            /* set encode result */
            if (m_callback != nullptr)
            {
                m_callback(m_userdata, (const char*)ptr, len);
            }

            if (out_ptr != nullptr && out_pkt.max_size > 0)
            {
                if (out_len + log_len < out_pkt.max_size)
                {
                    std::memcpy(out_ptr, ptr, len);
                    out_len += len;
                    out_ptr = (char*)out_ptr + len;
                }
                else
                {
                    std::cerr << "error enc_buf no enough" << std::endl;
                }
            }


            /* for low delay partition encoding */
            if (mpp_packet_is_partition(packet))
            {
                eoi = mpp_packet_is_eoi(packet);

            }

            log_len += snprintf(log_buf + log_len, log_size - log_len,
                                " size %-7zu", len);

            if (mpp_packet_has_meta(packet))
            {
                meta = mpp_packet_get_meta(packet);
                RK_S32 temporal_id = 0;
                RK_S32 lt_idx = -1;
                RK_S32 avg_qp = -1;

                if (MPP_OK == mpp_meta_get_s32(meta, KEY_TEMPORAL_ID, &temporal_id))
                {
                    log_len += snprintf(log_buf + log_len, log_size - log_len, " tid %d", temporal_id);
                }

                if (MPP_OK == mpp_meta_get_s32(meta, KEY_LONG_REF_IDX, &lt_idx))
                {
                    log_len += snprintf(log_buf + log_len, log_size - log_len, " lt %d", lt_idx);
                }

                if (MPP_OK == mpp_meta_get_s32(meta, KEY_ENC_AVERAGE_QP, &avg_qp))
                {
                    log_len += snprintf(log_buf + log_len, log_size - log_len, " qp %d", avg_qp);
                }
            }
            mpp_packet_deinit(&packet);
        }
    }
    while (!eoi);
    out_pkt.pkt_len = out_len;
    return out_len;
}

int rkmppEnc::getEncoderHeader(std::string& headBuf)
{
    int ret;
    size_t out_len{0};

    if (m_params.encoding_type == MPP_VIDEO_CodingAVC || m_params.encoding_type == MPP_VIDEO_CodingHEVC)
    {
        MppPacket packet = nullptr;

        /*
         * Can use packet with normal malloc buffer as input not pkt_buf.
         * Please refer to vpu_api_legacy.cpp for normal buffer case.
         * Using pkt_buf buffer here is just for simplifing demo.
         */
        mpp_packet_init_with_buffer(&packet, m_ctx.pkt_buf);
        /* NOTE: It is important to clear output packet length!! */
        mpp_packet_set_length(packet, 0);

        ret = m_ctx.mpi->control(m_ctx.mpp_ctx, MPP_ENC_GET_HDR_SYNC, packet);

        if (MPP_SUCCESS != ret)
        {
            std::cerr << "mpi control enc get extra info failed" << std::endl;
            return -1;
        }
        else
        {
            /* get and write sps/pps for H.264 */
            void *ptr   = mpp_packet_get_pos(packet);
            size_t len  = mpp_packet_get_length(packet);

            headBuf.assign(static_cast<char*>(ptr), len);
            out_len += len;
        }

        mpp_packet_deinit(&packet);
    }
    return out_len;
}

int rkmppEnc::tearDown()
{
    if (m_ctx.mpi != nullptr)
    {
        m_ctx.mpi->reset(m_ctx.mpp_ctx);
    }
    return 0;
}

} // namespace bsp_codec