#include <array>
#include <bsp_image/OpenCvImageAdapter.hpp>
#include <iostream>
#include <opencv2/imgcodecs.hpp>
#include <svs/ISurroundView.hpp>

namespace
{
std::string joinPath(const std::string& root, const std::string& path)
{
    if (path.empty() || root.empty() || path.front() == '/') {
        return path;
    }
    if (root.back() == '/') {
        return root + path;
    }
    return root + "/" + path;
}
} // namespace

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <data_root> [output.png]\n";
        return -1;
    }

    const std::string dataRoot = argv[1];
    const std::string outputPath = argc > 2 ? argv[2] : "svs_output.png";

    bsp_perf::svs::SurroundViewConfig config;
    config.dataRoot = dataRoot;
    config.weightImagePath = "yaml/weights.png";
    config.carImagePath = "images/car.png";
    config.cameras = {{
        {"front", "yaml/front.yaml", "n"},
        {"left", "yaml/left.yaml", "r-"},
        {"back", "yaml/back.yaml", "m"},
        {"right", "yaml/right.yaml", "r+"},
    }};

    bsp_perf::svs::FrameSet input;
    std::array<cv::Mat, bsp_perf::svs::kCameraCount> inputImages;
    std::array<bsp_perf::bsp_image::ImageBuffer, bsp_perf::svs::kCameraCount> inputBuffers;
    for (size_t i = 0; i < bsp_perf::svs::kCameraCount; ++i) {
        const auto& camera = config.cameras[i];
        inputImages[i] = cv::imread(joinPath(dataRoot, "images/" + camera.name + ".png"), cv::IMREAD_COLOR);
        if (inputImages[i].empty()) {
            std::cerr << "failed to read camera image: " << camera.name << "\n";
            return -1;
        }
        if (!bsp_perf::bsp_image::OpenCvImageAdapter::fromMat(inputImages[i], "BGR888", inputBuffers[i])) {
            std::cerr << "failed to wrap camera image: " << camera.name << "\n";
            return -1;
        }
        input.cameras[i] = inputBuffers[i].view;
    }

    auto svs = bsp_perf::svs::ISurroundView::create("opencv");
    if (svs->setup(config) != 0) {
        std::cerr << "failed to setup surround view\n";
        return -1;
    }

    bsp_perf::svs::OutputFrame output;
    if (svs->process(input, output) != 0 || output.image.view.empty())
    {
        std::cerr << "failed to process surround view\n";
        return -1;
    }

    cv::Mat outputImage;
    if (!bsp_perf::bsp_image::OpenCvImageAdapter::toMat(output.image.view, outputImage) ||
        !cv::imwrite(outputPath, outputImage))
    {
        std::cerr << "failed to write output: " << outputPath << "\n";
        return -1;
    }

    std::cout << "wrote surround view image: " << outputPath << "\n";
    return 0;
}
