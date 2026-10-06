#ifndef __IGRAPHICS2D_JOB_HPP__
#define __IGRAPHICS2D_JOB_HPP__

#include <bsp_g2d/IGraphics2D.hpp>

#include <memory>

namespace bsp_g2d
{

class IGraphics2DJob
{
public:
    struct Options
    {
        Options(bool async_ = false, int acquireFenceFd_ = 0, int* releaseFenceFd_ = nullptr)
            : async(async_)
            , acquireFenceFd(acquireFenceFd_)
            , releaseFenceFd(releaseFenceFd_)
        {
        }

        bool async;
        int acquireFenceFd;
        int* releaseFenceFd;
    };

    virtual int addBlitTask(
        std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> src,
        std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> dst,
        const IGraphics2D::TransformParams& params) = 0;

    virtual int addFillTask(
        std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> dst,
        const IGraphics2D::ImageRect& rect,
        uint32_t color) = 0;

    virtual int addBlendTask(
        std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> src,
        std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> dst,
        IGraphics2D::BlendMode mode = IGraphics2D::BlendMode::SrcOver) = 0;

    virtual int submit(const Options& options = Options()) = 0;

    virtual int wait() = 0;

    virtual int cancel() = 0;

    virtual ~IGraphics2DJob() = default;

protected:
    IGraphics2DJob() = default;
    IGraphics2DJob(const IGraphics2DJob&) = delete;
    IGraphics2DJob& operator=(const IGraphics2DJob&) = delete;
};

} // namespace bsp_g2d

#endif // __IGRAPHICS2D_JOB_HPP__
