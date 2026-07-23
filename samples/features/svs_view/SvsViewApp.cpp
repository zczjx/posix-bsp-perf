#include <array>
#include <bsp_image/OpenCvImageAdapter.hpp>
#include <iostream>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <svs/ISurroundView.hpp>

namespace
{
std::string joinPath(const std::string& root, const std::string& path)
{
    if (root.empty() || path.empty() || path.front() == '/') {
        return path;
    }
    return root.back() == '/' ? root + path : root + "/" + path;
}
} // namespace

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <images_root> [output.png]\n";
        return -1;
    }

    const std::string imagesRoot = argv[1];
    const std::string outputPath = argc > 2 ? argv[2] : "svs_output.png";

    bsp_perf::svs::SurroundViewConfig config;
    config.cameras = {{
        {"front", {2.3, 0.0, 2.2, 0.0, -35.0, 0.0}, 110.0},
        {"rear", {-2.3, 0.0, 2.2, 0.0, -35.0, 180.0}, 110.0},
        {"left", {0.0, -1.0, 2.2, 0.0, -35.0, -90.0}, 110.0},
        {"right", {0.0, 1.0, 2.2, 0.0, -35.0, 90.0}, 110.0},
    }};
    config.groundExtent = {7.0, 7.0, 12.5, 12.5};

    bsp_perf::svs::FrameSet input;
    std::array<cv::Mat, bsp_perf::svs::kCameraCount> inputImages;
    std::array<bsp_perf::bsp_image::ImageBuffer, bsp_perf::svs::kCameraCount> inputBuffers;
    for (size_t i = 0; i < bsp_perf::svs::kCameraCount; ++i) {
        const auto& camera = config.cameras[i];
        inputImages[i] = cv::imread(
            joinPath(imagesRoot, camera.name + ".png"), cv::IMREAD_COLOR);
        if (inputImages[i].empty() ||
            !bsp_perf::bsp_image::OpenCvImageAdapter::fromMat(
                inputImages[i], "BGR888", inputBuffers[i])) {
            std::cerr << "failed to load camera image: " << camera.name << "\n";
            return -1;
        }
        input.cameras[i] = inputBuffers[i].view;
    }

    auto svs = bsp_perf::svs::ISurroundView::create("geometry_bev");
    if (svs->setup(config) != 0) {
        std::cerr << "failed to setup surround view\n";
        return -1;
    }

    bsp_perf::svs::OutputFrame output;
    if (svs->process(input, output) != 0 || output.image.view.empty()) {
        std::cerr << "failed to process surround view\n";
        return -1;
    }

    cv::Mat rgbOutput;
    cv::Mat bgrOutput;
    if (!bsp_perf::bsp_image::OpenCvImageAdapter::toMat(output.image.view, rgbOutput)) {
        std::cerr << "failed to access surround view output\n";
        return -1;
    }
    cv::cvtColor(rgbOutput, bgrOutput, cv::COLOR_RGB2BGR);
    if (!cv::imwrite(outputPath, bgrOutput)) {
        std::cerr << "failed to write output: " << outputPath << "\n";
        return -1;
    }

    std::cout << "wrote surround view image: " << outputPath << "\n";
    return 0;
}
