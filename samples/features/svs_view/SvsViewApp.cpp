#include <array>
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

bsp_perf::bsp_image::ImageView toImageView(cv::Mat& image, const std::string& format)
{
    bsp_perf::bsp_image::ImageView view;
    view.desc.width = static_cast<uint32_t>(image.cols);
    view.desc.height = static_cast<uint32_t>(image.rows);
    view.desc.widthStride = static_cast<uint32_t>(image.cols);
    view.desc.heightStride = static_cast<uint32_t>(image.rows);
    view.desc.format = format;
    view.desc.dataSize = image.total() * image.elemSize();
    view.memoryType = bsp_perf::bsp_image::ImageMemoryType::Host;
    view.access = bsp_perf::bsp_image::ImageAccess::ReadWrite;
    view.planeCount = 1;
    view.planes[0].data = image.data;
    view.planes[0].size = view.desc.dataSize;
    view.planes[0].rowStride = static_cast<uint32_t>(image.step);
    return view;
}

cv::Mat toBgrMat(const bsp_perf::bsp_image::ImageView& view)
{
    if (view.desc.format != "BGR888" || view.data() == nullptr) {
        return {};
    }
    const size_t step = view.planes[0].rowStride > 0 ? view.planes[0].rowStride : view.desc.width * 3;
    return cv::Mat(static_cast<int>(view.desc.height),
                   static_cast<int>(view.desc.width),
                   CV_8UC3,
                   view.planes[0].data,
                   step);
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
    for (size_t i = 0; i < bsp_perf::svs::kCameraCount; ++i) {
        const auto& camera = config.cameras[i];
        inputImages[i] = cv::imread(joinPath(dataRoot, "images/" + camera.name + ".png"), cv::IMREAD_COLOR);
        if (inputImages[i].empty()) {
            std::cerr << "failed to read camera image: " << camera.name << "\n";
            return -1;
        }
        input.cameras[i] = toImageView(inputImages[i], "BGR888");
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

    cv::Mat outputImage = toBgrMat(output.image.view);
    if (outputImage.empty() || !cv::imwrite(outputPath, outputImage))
    {
        std::cerr << "failed to write output: " << outputPath << "\n";
        return -1;
    }

    std::cout << "wrote surround view image: " << outputPath << "\n";
    return 0;
}
