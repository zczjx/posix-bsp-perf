#include "RgaJob.hpp"
#include "RgaUtils.hpp"

#include <bsp_g2d/impl/G2DBufferInternal.hpp>
#include <rga/im2d_task.h>

#include <utility>

namespace bsp_g2d
{

using namespace rga_detail;

RgaJob::RgaJob(im_job_handle_t nativeHandle)
    : m_nativeHandle(nativeHandle)
{
}

RgaJob::~RgaJob()
{
    if (!m_submitted && m_nativeHandle) {
        imcancelJob(m_nativeHandle);
    } else if (m_releaseFenceFd >= 0) {
        imsync(m_releaseFenceFd);
    }
}

int RgaJob::addBlitTask(
    std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> src,
    std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> dst,
    const IGraphics2D::TransformParams& params)
{
    auto srcBuffer = impl::getG2DBufferInternal(src);
    auto dstBuffer = impl::getG2DBufferInternal(dst);
    if (!m_nativeHandle || !srcBuffer || !dstBuffer || m_submitted) {
        return -1;
    }

    const uint64_t scheduler = m_requiresRga2
        ? IM_SCHEDULER_RGA2_CORE0
        : IM_SCHEDULER_RGA3_CORE0 | IM_SCHEDULER_RGA3_CORE1;
    if (setScheduler(scheduler) != 0) {
        return -1;
    }
    im_rect srcRect = toImRect(params.srcRect);
    im_rect dstRect = toImRect(params.dstRect);
    im_rect patRect{};
    rga_buffer_t pat{};
    im_opt_t opt{};
    opt.interp = toInterpolation(params.interpolation);
    const int usage = toTransformUsage(params.rotation, params.flip);
    IM_STATUS ret = improcessTask(
        m_nativeHandle,
        getRgaBuffer(srcBuffer),
        getRgaBuffer(dstBuffer),
        pat,
        srcRect,
        dstRect,
        patRect,
        &opt,
        usage);
    if (!isOk(ret)) {
        return normalizeStatus(ret, "RgaJob addBlitTask");
    }

    m_buffers.push_back(std::move(src));
    m_buffers.push_back(std::move(dst));
    return 0;
}

int RgaJob::addFillTask(
    std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> dst,
    const IGraphics2D::ImageRect& rect,
    uint32_t color)
{
    auto dstBuffer = impl::getG2DBufferInternal(dst);
    if (!m_nativeHandle || !dstBuffer || m_submitted) {
        return -1;
    }

    m_requiresRga2 = true;
    if (setScheduler(IM_SCHEDULER_RGA2_CORE0) != 0) {
        return -1;
    }
    IM_STATUS ret = imfillTask(m_nativeHandle, getRgaBuffer(dstBuffer), toImRect(rect), color);
    if (!isOk(ret)) {
        return normalizeStatus(ret, "RgaJob addFillTask");
    }

    m_buffers.push_back(std::move(dst));
    return 0;
}

int RgaJob::addBlendTask(
    std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> src,
    std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> dst,
    IGraphics2D::BlendMode mode)
{
    auto srcBuffer = impl::getG2DBufferInternal(src);
    auto dstBuffer = impl::getG2DBufferInternal(dst);
    if (!m_nativeHandle || !srcBuffer || !dstBuffer || m_submitted) {
        return -1;
    }

    const uint64_t scheduler = m_requiresRga2
        ? IM_SCHEDULER_RGA2_CORE0
        : IM_SCHEDULER_RGA3_CORE0 | IM_SCHEDULER_RGA3_CORE1;
    if (setScheduler(scheduler) != 0) {
        return -1;
    }
    IM_STATUS ret = imblendTask(
        m_nativeHandle,
        getRgaBuffer(srcBuffer),
        getRgaBuffer(dstBuffer),
        toBlendMode(mode));
    if (!isOk(ret)) {
        return normalizeStatus(ret, "RgaJob addBlendTask");
    }

    m_buffers.push_back(std::move(src));
    m_buffers.push_back(std::move(dst));
    return 0;
}

int RgaJob::submit(const Options& options)
{
    if (!m_nativeHandle || m_submitted) {
        return -1;
    }

    const uint64_t scheduler = m_requiresRga2
        ? IM_SCHEDULER_RGA2_CORE0
        : IM_SCHEDULER_RGA3_CORE0 | IM_SCHEDULER_RGA3_CORE1;
    if (setScheduler(scheduler) != 0) {
        return -1;
    }

    int localFence = -1;
    int* fenceOut = options.releaseFenceFd != nullptr ? options.releaseFenceFd : &localFence;
    IM_STATUS ret = imendJob(
        m_nativeHandle,
        options.async ? IM_ASYNC : IM_SYNC,
        options.acquireFenceFd,
        fenceOut);
    if (!isOk(ret)) {
        return normalizeStatus(ret, "RgaJob submit");
    }

    m_submitted = true;
    if (options.async) {
        m_releaseFenceFd = options.releaseFenceFd != nullptr ? *options.releaseFenceFd : localFence;
    } else {
        m_buffers.clear();
    }
    m_nativeHandle = 0;
    return 0;
}

int RgaJob::wait()
{
    if (!m_submitted) {
        return -1;
    }
    if (m_releaseFenceFd < 0) {
        return 0;
    }

    int ret = normalizeStatus(imsync(m_releaseFenceFd), "RgaJob wait");
    m_releaseFenceFd = -1;
    m_buffers.clear();
    return ret;
}

int RgaJob::cancel()
{
    IM_STATUS ret = IM_STATUS_SUCCESS;
    if (!m_submitted && m_nativeHandle) {
        ret = imcancelJob(m_nativeHandle);
    } else if (m_releaseFenceFd >= 0) {
        ret = imsync(m_releaseFenceFd);
        m_releaseFenceFd = -1;
    }

    m_nativeHandle = 0;
    m_submitted = true;
    m_buffers.clear();
    return normalizeStatus(ret, "RgaJob cancel");
}

} // namespace bsp_g2d
