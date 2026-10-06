#include <bsp_g2d/IGraphics2D.hpp>
#include <bsp_g2d/IGraphics2DJob.hpp>
#include <bsp_image/ImageBuffer.hpp>
#include <bsp_image/OpenCvImageAdapter.hpp>
#include <shared/ArgParser.hpp>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#if defined(BUILD_PLATFORM_RK35XX)
#include <fcntl.h>
#include <linux/dma-heap.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace
{

using bsp_g2d::IGraphics2D;
using bsp_g2d::IGraphics2DJob;
using bsp_perf::bsp_image::ImageBuffer;
using bsp_perf::bsp_image::ImageDesc;
using bsp_perf::bsp_image::OpenCvImageAdapter;

struct OpResult
{
    std::string name;
    std::string status;
    double psnr{0.0};
    std::string note;
};

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

cv::Mat loadRgbaInput(const std::string& path)
{
    cv::Mat input = cv::imread(path, cv::IMREAD_UNCHANGED);
    if (input.empty()) {
        return {};
    }

    cv::Mat rgba;
    if (input.channels() == 4) {
        cv::cvtColor(input, rgba, cv::COLOR_BGRA2RGBA);
    } else if (input.channels() == 3) {
        cv::cvtColor(input, rgba, cv::COLOR_BGR2RGBA);
    } else if (input.channels() == 1) {
        cv::cvtColor(input, rgba, cv::COLOR_GRAY2RGBA);
    } else {
        return {};
    }

    const int targetWidth = std::max(128, alignUp(rgba.cols, 16));
    const int targetHeight = std::max(128, alignUp(rgba.rows, 2));
    if (targetWidth != rgba.cols || targetHeight != rgba.rows) {
        cv::copyMakeBorder(
            rgba,
            rgba,
            0,
            targetHeight - rgba.rows,
            0,
            targetWidth - rgba.cols,
            cv::BORDER_CONSTANT,
            cv::Scalar(0, 0, 0, 255));
    }
    return rgba;
}

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

cv::Mat matFromBuffer(const std::shared_ptr<ImageBuffer>& buffer)
{
    cv::Mat mat;
    if (!buffer || !OpenCvImageAdapter::toMat(buffer->view, mat)) {
        return {};
    }
    return mat.clone();
}

bool saveRgba(const std::filesystem::path& path, const std::shared_ptr<ImageBuffer>& buffer)
{
    cv::Mat rgba = matFromBuffer(buffer);
    if (rgba.empty()) {
        return false;
    }
    cv::Mat bgra;
    cv::cvtColor(rgba, bgra, cv::COLOR_RGBA2BGRA);
    return cv::imwrite(path.string(), bgra);
}

double computePsnr(const cv::Mat& a, const cv::Mat& b)
{
    if (a.empty() || b.empty() || a.size() != b.size() || a.type() != b.type()) {
        return 0.0;
    }
    cv::Mat diff;
    cv::absdiff(a, b, diff);
    diff.convertTo(diff, CV_32F);
    diff = diff.mul(diff);
    const cv::Scalar sum = cv::sum(diff);
    const double sse = sum[0] + sum[1] + sum[2] + sum[3];
    if (sse <= 1e-10) {
        return 100.0;
    }
    const double mse = sse / static_cast<double>(a.total() * a.channels());
    return 10.0 * std::log10((255.0 * 255.0) / mse);
}

OpResult finishOp(
    const std::string& name,
    int ret,
    const std::shared_ptr<ImageBuffer>& output,
    const cv::Mat& reference,
    const std::filesystem::path& outputDir,
    double threshold = 28.0)
{
    OpResult result{name, "FAIL", 0.0, ""};
    if (ret != 0) {
        result.note = "g2d returned failure";
        return result;
    }

    const auto imagePath = outputDir / (name + ".png");
    saveRgba(imagePath, output);
    const cv::Mat actual = matFromBuffer(output);
    result.psnr = computePsnr(actual, reference);
    result.status = result.psnr >= threshold ? "PASS" : "FAIL";
    return result;
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
    parser.addOption("--g2d", std::string("rkrga"), "G2D backend: rkrga or nvvic");
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
        std::cerr << "--image is required" << std::endl;
        return 2;
    }

    const cv::Mat inputRgba = loadRgbaInput(imagePath);
    if (inputRgba.empty()) {
        std::cerr << "failed to load input image: " << imagePath << std::endl;
        return 2;
    }

    std::filesystem::path outputDir(outputDirText);
    std::filesystem::create_directories(outputDir);
    cv::Mat inputBgra;
    cv::cvtColor(inputRgba, inputBgra, cv::COLOR_RGBA2BGRA);
    cv::imwrite((outputDir / "input.png").string(), inputBgra);

    auto g2d = IGraphics2D::create(g2dName);
    auto inputHost = bufferFromRgbaMat(inputRgba);
    auto input = createG2dBuffer(*g2d, inputHost);
    if (!input) {
        std::cerr << "failed to create input g2d buffer" << std::endl;
        return 2;
    }

    const int w = inputRgba.cols;
    const int h = inputRgba.rows;
    std::vector<OpResult> results;

    {
        auto outHost = makeImageBuffer(w, h, "RGBA8888");
        auto out = createG2dBuffer(*g2d, outHost);
        int ret = g2d->imageCopy(input, out);
        copyBackIfNeeded(*g2d, out);
        results.push_back(finishOp("copy", ret, out, inputRgba, outputDir, 60.0));
    }

    {
        const int rw = alignUp(w / 2, 4);
        const int rh = alignUp(h / 2, 2);
        auto outHost = makeImageBuffer(rw, rh, "RGBA8888");
        auto out = createG2dBuffer(*g2d, outHost);
        int ret = g2d->imageResize(input, out);
        copyBackIfNeeded(*g2d, out);
        cv::Mat ref;
        cv::resize(inputRgba, ref, cv::Size(rw, rh), 0, 0, cv::INTER_LINEAR);
        results.push_back(finishOp("resize", ret, out, ref, outputDir, psnrThreshold));
    }

    {
        IGraphics2D::ImageRect crop{w / 4, h / 4, alignUp(w / 2, 4), alignUp(h / 2, 2)};
        auto outHost = makeImageBuffer(crop.width, crop.height, "RGBA8888");
        auto out = createG2dBuffer(*g2d, outHost);
        int ret = g2d->imageCrop(input, out, crop);
        copyBackIfNeeded(*g2d, out);
        cv::Mat ref = inputRgba(cv::Rect(crop.x, crop.y, crop.width, crop.height)).clone();
        results.push_back(finishOp("crop", ret, out, ref, outputDir, psnrThreshold));
    }

    {
        auto outHost = makeImageBuffer(w, h, "RGBA8888");
        auto out = createG2dBuffer(*g2d, outHost);
        int ret = g2d->imageFill(out, {0, 0, w, h}, 0xff000000);
        IGraphics2D::TransformParams params{};
        params.dstRect = {w / 4, h / 4, alignUp(w / 2, 4), alignUp(h / 2, 2)};
        params.interpolation = IGraphics2D::Interpolation::Linear;
        ret = ret == 0 ? g2d->imageBlit(input, out, params) : ret;
        copyBackIfNeeded(*g2d, out);
        cv::Mat ref(h, w, CV_8UC4, cv::Scalar(0, 0, 0, 255));
        cv::Mat resized;
        cv::resize(inputRgba, resized, cv::Size(params.dstRect.width, params.dstRect.height), 0, 0, cv::INTER_LINEAR);
        resized.copyTo(ref(cv::Rect(params.dstRect.x, params.dstRect.y, params.dstRect.width, params.dstRect.height)));
        results.push_back(finishOp("resize_to_rect", ret, out, ref, outputDir, psnrThreshold));
    }

    if (g2d->queryCapability("fill")) {
        auto outHost = bufferFromRgbaMat(inputRgba);
        auto out = createG2dBuffer(*g2d, outHost);
        IGraphics2D::ImageRect rect{w / 4, h / 4, alignUp(w / 4, 4), alignUp(h / 4, 2)};
        int ret = g2d->imageFill(out, rect, 0xff00ff00);
        copyBackIfNeeded(*g2d, out);
        cv::Mat ref = inputRgba.clone();
        cv::rectangle(ref, cv::Rect(rect.x, rect.y, rect.width, rect.height), cv::Scalar(0, 255, 0, 255), cv::FILLED);
        results.push_back(finishOp("fill", ret, out, ref, outputDir, psnrThreshold));
    } else {
        addSkip(results, "fill", "capability not supported");
    }

    {
        auto outHost = bufferFromRgbaMat(inputRgba);
        auto out = createG2dBuffer(*g2d, outHost);
        IGraphics2D::ImageRect rect{w / 5, h / 5, alignUp(w / 3, 4), alignUp(h / 3, 2)};
        int ret = g2d->imageDrawRectangle(out, rect, 0xffff0000, 4);
        copyBackIfNeeded(*g2d, out);
        cv::Mat ref = inputRgba.clone();
        cv::rectangle(ref, cv::Rect(rect.x, rect.y, rect.width, rect.height), cv::Scalar(255, 0, 0, 255), 4);
        results.push_back(finishOp("rectangle", ret, out, ref, outputDir, psnrThreshold));
    }

    if (g2d->queryCapability("blend")) {
        cv::Mat overlay(inputRgba.size(), CV_8UC4, cv::Scalar(0, 0, 0, 0));
        cv::rectangle(overlay, cv::Rect(w / 4, h / 4, w / 2, h / 2), cv::Scalar(255, 0, 0, 128), cv::FILLED);
        auto overlayHost = bufferFromRgbaMat(overlay);
        auto overlayBuffer = createG2dBuffer(*g2d, overlayHost);
        auto outHost = bufferFromRgbaMat(inputRgba);
        auto out = createG2dBuffer(*g2d, outHost);
        int ret = g2d->imageBlend(overlayBuffer, out, IGraphics2D::BlendMode::SrcOver);
        copyBackIfNeeded(*g2d, out);
        cv::Mat ref = inputRgba.clone();
        for (int y = 0; y < ref.rows; ++y) {
            for (int x = 0; x < ref.cols; ++x) {
                const cv::Vec4b fg = overlay.at<cv::Vec4b>(y, x);
                if (fg[3] == 0) {
                    continue;
                }
                cv::Vec4b& bg = ref.at<cv::Vec4b>(y, x);
                const float a = fg[3] / 255.0F;
                for (int c = 0; c < 3; ++c) {
                    bg[c] = static_cast<uint8_t>(fg[c] * a + bg[c] * (1.0F - a));
                }
                bg[3] = 255;
            }
        }
        results.push_back(finishOp("blend", ret, out, ref, outputDir, psnrThreshold));
    } else {
        addSkip(results, "blend", "capability not supported");
    }

    const std::vector<std::pair<std::string, IGraphics2D::Rotation>> rotations = {
        {"rotate90", IGraphics2D::Rotation::Rotate90},
        {"rotate180", IGraphics2D::Rotation::Rotate180},
        {"rotate270", IGraphics2D::Rotation::Rotate270},
    };
    for (const auto& item : rotations) {
        const bool swap = item.second == IGraphics2D::Rotation::Rotate90 || item.second == IGraphics2D::Rotation::Rotate270;
        auto outHost = makeImageBuffer(swap ? h : w, swap ? w : h, "RGBA8888");
        auto out = createG2dBuffer(*g2d, outHost);
        IGraphics2D::TransformParams params{};
        params.rotation = item.second;
        int ret = g2d->imageBlit(input, out, params);
        copyBackIfNeeded(*g2d, out);
        cv::Mat ref;
        int rotateCode = cv::ROTATE_90_CLOCKWISE;
        if (item.second == IGraphics2D::Rotation::Rotate180) {
            rotateCode = cv::ROTATE_180;
        } else if (item.second == IGraphics2D::Rotation::Rotate270) {
            rotateCode = cv::ROTATE_90_COUNTERCLOCKWISE;
        }
        cv::rotate(inputRgba, ref, rotateCode);
        results.push_back(finishOp(item.first, ret, out, ref, outputDir, psnrThreshold));
    }

    const std::vector<std::pair<std::string, IGraphics2D::FlipMode>> flips = {
        {"flip_h", IGraphics2D::FlipMode::Horizontal},
        {"flip_v", IGraphics2D::FlipMode::Vertical},
        {"flip_hv", IGraphics2D::FlipMode::Both},
    };
    for (const auto& item : flips) {
        auto outHost = makeImageBuffer(w, h, "RGBA8888");
        auto out = createG2dBuffer(*g2d, outHost);
        IGraphics2D::TransformParams params{};
        params.flip = item.second;
        int ret = g2d->imageBlit(input, out, params);
        copyBackIfNeeded(*g2d, out);
        cv::Mat ref;
        int flipCode = item.second == IGraphics2D::FlipMode::Horizontal ? 1 : 0;
        if (item.second == IGraphics2D::FlipMode::Both) {
            flipCode = -1;
        }
        cv::flip(inputRgba, ref, flipCode);
        results.push_back(finishOp(item.first, ret, out, ref, outputDir, psnrThreshold));
    }

    {
        auto outHost = makeImageBuffer(w, h, "BGRA8888");
        auto out = createG2dBuffer(*g2d, outHost);
        int ret = g2d->imageCvtColor(input, out, "RGBA8888", "BGRA8888");
        copyBackIfNeeded(*g2d, out);
        cv::Mat ref;
        cv::cvtColor(inputRgba, ref, cv::COLOR_RGBA2BGRA);
        cv::Mat actual;
        OpenCvImageAdapter::toMat(out->view, actual);
        cv::imwrite((outputDir / "cvtcolor_bgra.png").string(), actual);
        OpResult result{"cvtcolor_bgra", ret == 0 ? "PASS" : "FAIL", ret == 0 ? computePsnr(actual, ref) : 0.0, ""};
        if (result.psnr < 60.0) {
            result.status = "FAIL";
        }
        results.push_back(result);
    }

    if (g2d->queryCapability("async_job")) {
        auto outHost = makeImageBuffer(alignUp(w / 2, 4), alignUp(h / 2, 2), "RGBA8888");
        auto out = createG2dBuffer(*g2d, outHost);
        auto job = g2d->createJob();
        IGraphics2D::TransformParams params{};
        int ret = job ? job->addBlitTask(input, out, params) : -1;
        ret = ret == 0 ? job->submit(IGraphics2DJob::Options(true)) : ret;
        ret = ret == 0 ? job->wait() : ret;
        copyBackIfNeeded(*g2d, out);
        cv::Mat ref;
        cv::resize(inputRgba, ref, cv::Size(out->view.desc.width, out->view.desc.height), 0, 0, cv::INTER_LINEAR);
        results.push_back(finishOp("async_job", ret, out, ref, outputDir, psnrThreshold));
    } else {
        addSkip(results, "async_job", "capability not supported");
    }

    if (g2d->queryCapability("async_job")) {
        auto outHost = makeImageBuffer(w, h, "RGBA8888");
        auto out = createG2dBuffer(*g2d, outHost);
        auto job = g2d->createJob();
        int ret = job ? job->addFillTask(out, {0, 0, w, h}, 0xff000000) : -1;
        IGraphics2D::TransformParams params{};
        params.dstRect = {w / 4, h / 4, alignUp(w / 2, 4), alignUp(h / 2, 2)};
        ret = ret == 0 ? job->addBlitTask(input, out, params) : ret;
        ret = ret == 0 ? job->submit() : ret;
        copyBackIfNeeded(*g2d, out);
        cv::Mat ref(h, w, CV_8UC4, cv::Scalar(0, 0, 0, 255));
        cv::Mat resized;
        cv::resize(inputRgba, resized, cv::Size(params.dstRect.width, params.dstRect.height), 0, 0, cv::INTER_LINEAR);
        resized.copyTo(ref(cv::Rect(params.dstRect.x, params.dstRect.y, params.dstRect.width, params.dstRect.height)));
        results.push_back(finishOp("batch_job", ret, out, ref, outputDir, psnrThreshold));
    } else {
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
