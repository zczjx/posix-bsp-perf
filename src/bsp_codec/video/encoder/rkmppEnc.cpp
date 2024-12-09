#include "rkmppEnc.hpp"
#include <iostream>
#include <sys/time.h>
#include <unistd.h>

namespace bsp_codec
{

rkmppEnc::rkmppEnc()
{

}

rkmppEnc::~rkmppEnc()
{

}


int rkmppEnc::parseConfig(EncodeConfig& cfg)
{
    // Implementation of parseConfig function
    std::cout << "Parsing encoder configuration." << std::endl;
    // Add your logic to parse encoder configuration here
    return 0;
}

int rkmppEnc::setupMppEncCfg()
{
    // Implementation of setupMppEncCfg function
    std::cout << "Setting up MPP encoder configuration." << std::endl;
    // Add your logic to setup MPP encoder configuration here
    return 0;
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
    std::cout << "encoder test start w " << m_params.width << " h " << m_params.height << " type " << m_params.type << std::endl;

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

int rkmppEnc::encode(EncodeFrame& frame_data)
{
    MPP_RET ret;
    void* out_ptr = enc_buf;
    size_t out_len = 0;

    MppMeta meta = NULL;
    MppFrame frame = NULL;
    MppPacket packet = NULL;
    // void *buf = mpp_buffer_get_ptr(this->frm_buf);
    // RK_S32 cam_frm_idx = -1;
    // MppBuffer cam_buf = NULL;
    RK_U32 eoi = 1;
    RK_U32 frm_eos = 0;

    ret = mpp_frame_init(&frame);
    if (ret)
    {
        LOGD("mpp_frame_init failed\n");
        return -1;
    }

    mpp_frame_set_width(frame, enc_params.width);
    mpp_frame_set_height(frame, enc_params.height);
    mpp_frame_set_hor_stride(frame, enc_params.hor_stride);
    mpp_frame_set_ver_stride(frame, enc_params.ver_stride);
    mpp_frame_set_fmt(frame, enc_params.fmt);
    mpp_frame_set_eos(frame, frm_eos);
    mpp_frame_set_buffer(frame, mpp_buf);

    meta = mpp_frame_get_meta(frame);
    mpp_packet_init_with_buffer(&packet, pkt_buf);
    /* NOTE: It is important to clear output packet length!! */
    mpp_packet_set_length(packet, 0);
    mpp_meta_set_packet(meta, KEY_OUTPUT_PACKET, packet);
    mpp_meta_set_buffer(meta, KEY_MOTION_INFO, this->md_info);

    /*
        * NOTE: in non-block mode the frame can be resent.
        * The default input timeout mode is block.
        *
        * User should release the input frame to meet the requirements of
        * resource creator must be the resource destroyer.
        */
    ret = mpp_mpi->encode_put_frame(mpp_ctx, frame);
    mpp_frame_deinit(&frame);

    if (ret) {
        LOGD("chn %d encode put frame failed\n", chn);
        return -1;
    }

    do {
        ret = mpp_mpi->encode_get_packet(mpp_ctx, &packet);
        if (ret) {
            LOGD("chn %d encode get packet failed\n", chn);
            return -1;
        }

        // mpp_assert(packet);

        if (packet) {
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
            if (this->callback != nullptr) {
                this->callback(this->userdata, (const char*)ptr, len);
            }
            if (enc_buf != nullptr && max_size > 0) {
                if (out_len + log_len < max_size) {
                    memcpy(out_ptr, ptr, len);
                    out_len += len;
                    out_ptr = (char*)out_ptr + len;
                } else {
                    LOGE("error enc_buf no enought");
                }
            }


            /* for low delay partition encoding */
            if (mpp_packet_is_partition(packet))
            {
                eoi = mpp_packet_is_eoi(packet);

            }

            log_len += snprintf(log_buf + log_len, log_size - log_len,
                                " size %-7zu", len);

            if (mpp_packet_has_meta(packet)) {
                meta = mpp_packet_get_meta(packet);
                RK_S32 temporal_id = 0;
                RK_S32 lt_idx = -1;
                RK_S32 avg_qp = -1;

                if (MPP_OK == mpp_meta_get_s32(meta, KEY_TEMPORAL_ID, &temporal_id))
                    log_len += snprintf(log_buf + log_len, log_size - log_len,
                                        " tid %d", temporal_id);

                if (MPP_OK == mpp_meta_get_s32(meta, KEY_LONG_REF_IDX, &lt_idx))
                    log_len += snprintf(log_buf + log_len, log_size - log_len,
                                        " lt %d", lt_idx);

                if (MPP_OK == mpp_meta_get_s32(meta, KEY_ENC_AVERAGE_QP, &avg_qp))
                    log_len += snprintf(log_buf + log_len, log_size - log_len,
                                        " qp %d", avg_qp);
            }

            LOGD("chn %d %s\n", chn, log_buf);

            mpp_packet_deinit(&packet);

        }
    }
    while (!eoi);

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

int rkmppEnc::reset()
{
    if (m_ctx.mpi != nullptr)
    {
        m_ctx.mpi->reset(m_ctx.mpp_ctx);
    }
    return 0;
}

} // namespace bsp_codec