#ifndef __RGA_JOB_HPP__
#define __RGA_JOB_HPP__

#include <bsp_g2d/IGraphics2DJob.hpp>
#include <rga/im2d.h>

#include <memory>
#include <vector>

namespace bsp_g2d
{

class RgaJob final : public IGraphics2DJob
{
public:
    explicit RgaJob(im_job_handle_t nativeHandle);
    ~RgaJob() override;

    int addBlitTask(
        std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> src,
        std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> dst,
        const IGraphics2D::TransformParams& params) override;

    int addFillTask(
        std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> dst,
        const IGraphics2D::ImageRect& rect,
        uint32_t color) override;

    int addBlendTask(
        std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> src,
        std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> dst,
        IGraphics2D::BlendMode mode = IGraphics2D::BlendMode::SrcOver) override;

    int submit(const Options& options = Options()) override;
    int wait() override;
    int cancel() override;

private:
    im_job_handle_t m_nativeHandle{};
    int m_releaseFenceFd{-1};
    bool m_submitted{false};
    bool m_requiresRga2{false};
    std::vector<std::shared_ptr<bsp_perf::bsp_image::ImageBuffer>> m_buffers;
};

} // namespace bsp_g2d

#endif // __RGA_JOB_HPP__
