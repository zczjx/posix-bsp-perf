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

    parser.addSubOption("objDetectParams", "--conf_threshold", float(0.25), "objDetectParams conf_threshold");
    parser.addSubOption("objDetectParams", "--nms_threshold", float(0.45), "objDetectParams nms_threshold");
    parser.addSubOption("objDetectParams", "--pads_left", int(0), "objDetectParams pads_left");
    parser.addSubOption("objDetectParams", "--pads_right", int(0), "objDetectParams pads_right");
    parser.addSubOption("objDetectParams", "--pads_top", int(0), "objDetectParams pads_top");
    parser.addSubOption("objDetectParams", "--pads_bottom", int(0), "objDetectParams pads_bottom");

    parser.setConfig("--cfg", "config.ini", "set an configuration ini file for all options");
    parser.parseArgs(argc, argv);


    VideoDetectApp app(std::move(parser));
    app.run();

    return 0;
}