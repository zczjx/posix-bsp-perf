#include "OpenCVUtils.hpp"

#include <bsp_g2d/IGraphics2D.hpp>
#include <bsp_g2d/IGraphics2DJob.hpp>
#include <bsp_image/ImageBuffer.hpp>
#include <shared/ArgParser.hpp>

#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#if defined(BUILD_PLATFORM_RK35XX)
#include <fcntl.h>
#include <linux/dma-heap.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

using namespace g2d_image_ops;

namespace
{

using bsp_g2d::IGraphics2D;
using bsp_g2d::IGraphics2DJob;
using bsp_perf::bsp_image::ImageBuffer;
using bsp_perf::bsp_image::ImageDesc;

int alignUp(int value, int alignment)
{
    return ((value + alignment - 1) / alignment) * alignment;
}

#if defined(BUILD_PLATFORM_RK35XX)
std::shared_ptr<ImageBuffer> makeDma32ImageBuffer(const ImageDesc& desc)
{
    static const char* heapPaths[] = {
        "/dev/dma_heap/system-uncached-dma32",
        "/dev/dma_heap/cma-uncached",
    };

    for (const char* heapPath : heapPaths) {
        const int heapFd = open(heapPath, O_RDWR | O_CLOEXEC);
        if (heapFd < 0) {
            continue;
        }

        dma_heap_allocation_data allocation{};
        allocation.len = desc.dataSize;
        allocation.fd_flags = O_RDWR | O_CLOEXEC;
        const int ret = ioctl(heapFd, DMA_HEAP_IOCTL_ALLOC, &allocation);
        close(heapFd);
        if (ret != 0) {
            continue;
        }

        const int dmaFd = static_cast<int>(allocation.fd);
        void* data = mmap(nullptr, desc.dataSize, PROT_READ | PROT_WRITE, MAP_SHARED, dmaFd, 0);
        if (data == MAP_FAILED) {
            close(dmaFd);
            continue;
        }

        std::memset(data, 0, desc.dataSize);
        auto buffer = std::make_shared<ImageBuffer>();
        buffer->owner = std::shared_ptr<void>(data, [size = desc.dataSize, dmaFd](void* ptr) {
            munmap(ptr, size);
            close(dmaFd);
        });
        const uint32_t rowStride = static_cast<uint32_t>(
            desc.widthStride * bsp_perf::bsp_image::bytesPerPixel(desc.format));
        buffer->view = bsp_perf::bsp_image::makeHostImageView(
            static_cast<uint8_t*>(data), desc, rowStride);
        buffer->view.memoryType = bsp_perf::bsp_image::ImageMemoryType::DmaBuf;
        buffer->view.planes[0].fd = dmaFd;
        return buffer;
    }

    return nullptr;
}
#endif

std::shared_ptr<ImageBuffer> makeImageBuffer(int width, int height, const std::string& format)
{
    ImageDesc desc{};
    desc.width = static_cast<uint32_t>(width);
    desc.height = static_cast<uint32_t>(height);
    desc.widthStride = static_cast<uint32_t>(width);
    desc.heightStride = static_cast<uint32_t>(height);
    desc.format = format;
    desc.dataSize = bsp_perf::bsp_image::imageDataSize(desc);
#if defined(BUILD_PLATFORM_RK35XX)
    if (auto buffer = makeDma32ImageBuffer(desc)) {
        return buffer;
    }
    std::cerr << "failed to allocate DMA32 image buffer" << std::endl;
    return nullptr;
#else
    auto buffer = bsp_perf::bsp_image::makeHostImageBuffer(desc);
    buffer->view.planes[0].rowStride =
        static_cast<uint32_t>(width * bsp_perf::bsp_image::bytesPerPixel(format));
    return buffer;
#endif
}

std::shared_ptr<ImageBuffer> bufferFromRgbaMat(const cv::Mat& rgba)
{
    auto buffer = makeImageBuffer(rgba.cols, rgba.rows, "RGBA8888");
    if (!buffer) {
        return nullptr;
    }
    const size_t rowBytes = static_cast<size_t>(rgba.cols) * rgba.elemSize();
    for (int row = 0; row < rgba.rows; ++row) {
        std::memcpy(
            buffer->view.planes[0].data + row * buffer->view.planes[0].rowStride,
            rgba.ptr(row),
            rowBytes);
    }
    return buffer;
}

void writeManifest(const std::filesystem::path& path, const std::vector<OpResult>& results)
{
    std::ofstream os(path);
    os << "{\n  \"results\": [\n";
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        os << "    {\"name\": \"" << r.name << "\", \"status\": \"" << r.status
           << "\", \"psnr\": " << r.psnr << ", \"note\": \"" << r.note << "\"}";
        if (i + 1 < results.size()) {
            os << ",";
        }
        os << "\n";
    }
    os << "  ]\n}\n";
}

std::shared_ptr<ImageBuffer> createG2dBuffer(
    IGraphics2D& g2d,
    const std::shared_ptr<ImageBuffer>& host)
{
    if (!host) {
        return nullptr;
    }
    const auto type = host->view.planes[0].fd >= 0
        ? IGraphics2D::BufferType::Hardware
        : IGraphics2D::BufferType::Mapped;
    return g2d.createBuffer(type, host->view);
}

void copyBackIfNeeded(IGraphics2D& g2d, const std::shared_ptr<ImageBuffer>& buffer)
{
    g2d.syncBuffer(buffer, IGraphics2D::SyncDirection::DeviceToCpu);
}

void addSkip(std::vector<OpResult>& results, const std::string& name, const std::string& note)
{
    results.push_back({name, "SKIP", 0.0, note});
}

} // namespace

int main(int argc, char* argv[])
{
    bsp_perf::shared::ArgParser parser("G2D single image API sample");
    parser.addOption("--image", std::string(""), "Path to input image");
    parser.addOption("--output-dir", std::string("./g2d_image_ops_out"), "Output directory");
#if defined(BUILD_PLATFORM_JETSON)
    const std::string defaultG2d = "nvvic";
#else
    const std::string defaultG2d = "rkrga";
#endif
    parser.addOption("--g2d", defaultG2d, "G2D backend: rkrga (RK3588) or nvvic (Jetson)");
    parser.addOption("--psnr-threshold", double(20.0), "PSNR pass threshold");
    parser.parseArgs(argc, argv);

    std::string imagePath;
    std::string outputDirText;
    std::string g2dName;
    double psnrThreshold = 20.0;
    parser.getOptionVal("--image", imagePath);
    parser.getOptionVal("--output-dir", outputDirText);
    parser.getOptionVal("--g2d", g2dName);
    parser.getOptionVal("--psnr-threshold", psnrThreshold);

    if (imagePath.empty()) {
        // 没有输入图无法跑回归，直接失败退出。
        std::cerr << "--image is required" << std::endl;
        return 2;
    }

    const cv::Mat inputRgba = loadRgbaInputOpenCV(imagePath);
    if (inputRgba.empty()) {
        // 读图或通道转换失败。
        std::cerr << "failed to load input image: " << imagePath << std::endl;
        return 2;
    }

    std::filesystem::path outputDir(outputDirText);
    std::filesystem::create_directories(outputDir);
    saveRgbaPngOpenCV(outputDir / "input.png", inputRgba);

    std::unique_ptr<IGraphics2D> g2d;
    try {
        // 按 --g2d 创建后端：RK3588 为 rkrga，Jetson 为 nvvic。
        g2d = IGraphics2D::create(g2dName);
    } catch (const std::exception& e) {
        // 后端名无效或设备初始化失败。
        std::cerr << "failed to create g2d backend '" << g2dName << "': " << e.what() << std::endl;
        return 2;
    }
    auto inputHost = bufferFromRgbaMat(inputRgba);
    auto input = createG2dBuffer(*g2d, inputHost);
    if (!input) {
        // 输入图没能登记成 G2D 可用的 buffer。
        std::cerr << "failed to create input g2d buffer" << std::endl;
        return 2;
    }

    const int w = inputRgba.cols;
    const int h = inputRgba.rows;
    std::vector<OpResult> results;

    {
        // 整图拷贝：imageCopy 到同尺寸 RGBA 缓冲，参考图就是原图，阈值 60 dB。
        auto outHost = makeImageBuffer(w, h, "RGBA8888");
        auto out = createG2dBuffer(*g2d, outHost);
        int ret = g2d->imageCopy(input, out);
        copyBackIfNeeded(*g2d, out);
        results.push_back(finishOpOpenCV("copy", ret, out, inputRgba, outputDir, 60.0));
    }

    {
        // 缩小到约一半（宽 4 对齐、高 2 对齐），参考为 OpenCV 线性缩放。
        const int rw = alignUp(w / 2, 4);
        const int rh = alignUp(h / 2, 2);
        auto outHost = makeImageBuffer(rw, rh, "RGBA8888");
        auto out = createG2dBuffer(*g2d, outHost);
        int ret = g2d->imageResize(input, out);
        copyBackIfNeeded(*g2d, out);
        results.push_back(finishOpOpenCV("resize", ret, out, resizeReferenceOpenCV(inputRgba, rw, rh), outputDir, psnrThreshold));
    }

    {
        // 从 (w/4, h/4) 裁出约一半区域，参考为原图上同一块 ROI。
        IGraphics2D::ImageRect crop{w / 4, h / 4, alignUp(w / 2, 4), alignUp(h / 2, 2)};
        auto outHost = makeImageBuffer(crop.width, crop.height, "RGBA8888");
        auto out = createG2dBuffer(*g2d, outHost);
        int ret = g2d->imageCrop(input, out, crop);
        copyBackIfNeeded(*g2d, out);
        const cv::Mat ref = cropReferenceOpenCV(inputRgba, crop.x, crop.y, crop.width, crop.height);
        results.push_back(finishOpOpenCV("crop", ret, out, ref, outputDir, psnrThreshold));
    }

    {
        // 先把输出铺成不透明黑，再把原图线性缩放到中心矩形并 blit 上去。
        auto outHost = makeImageBuffer(w, h, "RGBA8888");
        auto out = createG2dBuffer(*g2d, outHost);
        int ret = g2d->imageFill(out, {0, 0, w, h}, 0xff000000);
        IGraphics2D::TransformParams params{};
        params.dstRect = {w / 4, h / 4, alignUp(w / 2, 4), alignUp(h / 2, 2)};
        params.interpolation = IGraphics2D::Interpolation::Linear;
        ret = ret == 0 ? g2d->imageBlit(input, out, params) : ret;
        copyBackIfNeeded(*g2d, out);
        const cv::Mat ref = resizeOntoCanvasOpenCV(
            inputRgba,
            w,
            h,
            params.dstRect.x,
            params.dstRect.y,
            params.dstRect.width,
            params.dstRect.height);
        results.push_back(finishOpOpenCV("resize_to_rect", ret, out, ref, outputDir, psnrThreshold));
    }

    if (g2d->queryCapability("fill")) {
        // 在原图副本上填充一块不透明绿色矩形（0xff00ff00）。
        auto outHost = bufferFromRgbaMat(inputRgba);
        auto out = createG2dBuffer(*g2d, outHost);
        IGraphics2D::ImageRect rect{w / 4, h / 4, alignUp(w / 4, 4), alignUp(h / 4, 2)};
        int ret = g2d->imageFill(out, rect, 0xff00ff00);
        copyBackIfNeeded(*g2d, out);
        const cv::Mat ref = filledRectangleReferenceOpenCV(
            inputRgba, rect.x, rect.y, rect.width, rect.height, cv::Scalar(0, 255, 0, 255));
        results.push_back(finishOpOpenCV("fill", ret, out, ref, outputDir, psnrThreshold));
    } else {
        // 后端没有 fill 能力（例如 Jetson VIC），记为 SKIP。
        addSkip(results, "fill", "capability not supported");
    }

    {
        // 在原图副本上画红色描边矩形，线宽 4。
        auto outHost = bufferFromRgbaMat(inputRgba);
        auto out = createG2dBuffer(*g2d, outHost);
        IGraphics2D::ImageRect rect{w / 5, h / 5, alignUp(w / 3, 4), alignUp(h / 3, 2)};
        int ret = g2d->imageDrawRectangle(out, rect, 0xffff0000, 4);
        copyBackIfNeeded(*g2d, out);
        const cv::Mat ref = strokedRectangleReferenceOpenCV(
            inputRgba, rect.x, rect.y, rect.width, rect.height, cv::Scalar(255, 0, 0, 255), 4);
        results.push_back(finishOpOpenCV("rectangle", ret, out, ref, outputDir, psnrThreshold));
    }

    if (g2d->queryCapability("blend")) {
        // 做一块半透明红色蒙版，用 SrcOver 叠到原图上，参考为逐像素 alpha 混合。
        const cv::Mat overlay = makeRedBlendOverlayOpenCV(inputRgba);
        auto overlayHost = bufferFromRgbaMat(overlay);
        auto overlayBuffer = createG2dBuffer(*g2d, overlayHost);
        auto outHost = bufferFromRgbaMat(inputRgba);
        auto out = createG2dBuffer(*g2d, outHost);
        int ret = g2d->imageBlend(overlayBuffer, out, IGraphics2D::BlendMode::SrcOver);
        copyBackIfNeeded(*g2d, out);
        results.push_back(finishOpOpenCV(
            "blend", ret, out, srcOverReferenceOpenCV(inputRgba, overlay), outputDir, psnrThreshold));
    } else {
        // 后端没有 blend 能力，记为 SKIP。
        addSkip(results, "blend", "capability not supported");
    }

    const std::vector<std::pair<std::string, IGraphics2D::Rotation>> rotations = {
        {"rotate90", IGraphics2D::Rotation::Rotate90},
        {"rotate180", IGraphics2D::Rotation::Rotate180},
        {"rotate270", IGraphics2D::Rotation::Rotate270},
    };
    for (const auto& item : rotations) {
        // 分别测 90/180/270 顺时针旋转；90/270 会交换宽高。
        const bool swap = item.second == IGraphics2D::Rotation::Rotate90 || item.second == IGraphics2D::Rotation::Rotate270;
        auto outHost = makeImageBuffer(swap ? h : w, swap ? w : h, "RGBA8888");
        auto out = createG2dBuffer(*g2d, outHost);
        IGraphics2D::TransformParams params{};
        params.rotation = item.second;
        int ret = g2d->imageBlit(input, out, params);
        copyBackIfNeeded(*g2d, out);
        int degrees = 90;
        if (item.second == IGraphics2D::Rotation::Rotate180) {
            degrees = 180;
        } else if (item.second == IGraphics2D::Rotation::Rotate270) {
            degrees = 270;
        }
        results.push_back(finishOpOpenCV(
            item.first, ret, out, rotateReferenceOpenCV(inputRgba, degrees), outputDir, psnrThreshold));
    }

    const std::vector<std::pair<std::string, IGraphics2D::FlipMode>> flips = {
        {"flip_h", IGraphics2D::FlipMode::Horizontal},
        {"flip_v", IGraphics2D::FlipMode::Vertical},
        {"flip_hv", IGraphics2D::FlipMode::Both},
    };
    for (const auto& item : flips) {
        // 分别测水平、垂直、双向翻转。
        auto outHost = makeImageBuffer(w, h, "RGBA8888");
        auto out = createG2dBuffer(*g2d, outHost);
        IGraphics2D::TransformParams params{};
        params.flip = item.second;
        int ret = g2d->imageBlit(input, out, params);
        copyBackIfNeeded(*g2d, out);
        OpenCVAxisFlip axis = OpenCVAxisFlip::Vertical;
        if (item.second == IGraphics2D::FlipMode::Horizontal) {
            axis = OpenCVAxisFlip::Horizontal;
        } else if (item.second == IGraphics2D::FlipMode::Both) {
            axis = OpenCVAxisFlip::Both;
        }
        results.push_back(finishOpOpenCV(item.first, ret, out, flipReferenceOpenCV(inputRgba, axis), outputDir, psnrThreshold));
    }

    {
        // RGBA8888 转 BGRA8888。RGA 近乎无损，VIC 约 ±1 LSB，阈值单独用 50 dB。
        auto outHost = makeImageBuffer(w, h, "BGRA8888");
        auto out = createG2dBuffer(*g2d, outHost);
        int ret = g2d->imageCvtColor(input, out, "RGBA8888", "BGRA8888");
        copyBackIfNeeded(*g2d, out);
        results.push_back(finishCvtColorBgraOpenCV(ret, out, inputRgba, outputDir));
    }

    if (g2d->queryCapability("async_job")) {
        // 异步任务：提交一次缩放到一半的 blit，submit 后 wait 再读结果。
        auto outHost = makeImageBuffer(alignUp(w / 2, 4), alignUp(h / 2, 2), "RGBA8888");
        auto out = createG2dBuffer(*g2d, outHost);
        auto job = g2d->createJob();
        IGraphics2D::TransformParams params{};
        int ret = job ? job->addBlitTask(input, out, params) : -1;
        ret = ret == 0 ? job->submit(IGraphics2DJob::Options(true)) : ret;
        ret = ret == 0 ? job->wait() : ret;
        copyBackIfNeeded(*g2d, out);
        const cv::Mat ref = resizeReferenceOpenCV(inputRgba, out->view.desc.width, out->view.desc.height);
        results.push_back(finishOpOpenCV("async_job", ret, out, ref, outputDir, psnrThreshold));
    } else {
        // 后端没有 job 队列（例如 Jetson VIC），记为 SKIP。
        addSkip(results, "async_job", "capability not supported");
    }

    if (g2d->queryCapability("async_job")) {
        // 批处理任务：同一个 job 里先全图填黑，再把原图 blit 到中心矩形，同步提交。
        auto outHost = makeImageBuffer(w, h, "RGBA8888");
        auto out = createG2dBuffer(*g2d, outHost);
        auto job = g2d->createJob();
        int ret = job ? job->addFillTask(out, {0, 0, w, h}, 0xff000000) : -1;
        IGraphics2D::TransformParams params{};
        params.dstRect = {w / 4, h / 4, alignUp(w / 2, 4), alignUp(h / 2, 2)};
        ret = ret == 0 ? job->addBlitTask(input, out, params) : ret;
        ret = ret == 0 ? job->submit() : ret;
        copyBackIfNeeded(*g2d, out);
        const cv::Mat ref = resizeOntoCanvasOpenCV(
            inputRgba,
            w,
            h,
            params.dstRect.x,
            params.dstRect.y,
            params.dstRect.width,
            params.dstRect.height);
        results.push_back(finishOpOpenCV("batch_job", ret, out, ref, outputDir, psnrThreshold));
    } else {
        // 后端没有 job 队列，批处理记为 SKIP。
        addSkip(results, "batch_job", "capability not supported");
    }

    writeManifest(outputDir / "manifest.json", results);

    bool failed = false;
    for (const auto& result : results) {
        std::cout << result.name << ": " << result.status;
        if (result.status != "SKIP") {
            std::cout << " psnr=" << result.psnr;
        }
        if (!result.note.empty()) {
            std::cout << " (" << result.note << ")";
        }
        std::cout << std::endl;
        failed = failed || result.status == "FAIL";
    }
    return failed ? 1 : 0;
}
