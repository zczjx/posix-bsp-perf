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
    parser.addOption("--video, --inputVideoPath", std::string(""), "Path to the input video file");
    parser.addOption("--decoder, --decoderType", std::string("rkmpp"), "decoder type: rkmpp");
    parser.addOption("--encoder, --encoderType", std::string("rkmpp"), "decoder type: rkmpp");
    parser.addOption("--g2d, --graphics2D", std::string("rkrga"), "graphics 2d platform type: rkrga");
    parser.addOption("--output, --outputVideoPath", std::string("out.h264"), "Path to the output video file");
    parser.setConfig("--cfg", "config.ini", "set an configuration ini file for all options");
    parser.parseArgs(argc, argv);


    VideoDetectApp app(std::move(parser));
    app.run();

    return 0;
}