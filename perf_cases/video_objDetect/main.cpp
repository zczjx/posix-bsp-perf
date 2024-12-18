#include <iostream>
#include <string>
#include "VideoDetectApp.hpp"

using namespace bsp_perf::perf_cases;
using namespace bsp_perf::shared;

int main(int argc, char* argv[])
{
    ArgParser parser("VideoDetectApp");
    parser.addOption("--dnn, --dnnType", std::string("rknn"), "DNN type: trt or rknn");
    parser.addOption("--plugin, --pluginPath", std::string(""), "Path to the plugin library");
    parser.addOption("--label, --labelTextPath", std::string(""), "Path to the label text file");
    parser.addOption("--model, --modelPath", std::string(""), "Path to the model file");
    parser.addOption("--video, --videoPath", std::string(""), "Path to the input video file");
    parser.addOption("--decoder, --decoderType", std::string(""), "decoder type: rkmpp or v4l2");
    parser.addOption("--encoder, --encoderType", std::string(""), "decoder type: rkmpp or v4l2");
    parser.parseArgs(argc, argv);


    VideoDetectApp app(std::move(parser));
    app.run();

    return 0;
}