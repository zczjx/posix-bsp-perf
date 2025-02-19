#include <iostream>
#include <string>
#include "objDetectApp.hpp"

using namespace bsp_perf::perf_cases;
using namespace bsp_perf::shared;

int main(int argc, char* argv[])
{
    ArgParser parser("ObjDetectApp");
    parser.addOption("--dnn, --dnnType", std::string("rknn"), "DNN type: trt or rknn");
    parser.addOption("--plugin, --pluginPath", std::string(""), "Path to the plugin library");
    parser.addOption("--plugin, --pluginType", std::string(""), "Type of the plugin, yolov5 or yolov8");
    parser.addOption("--label, --labelTextPath", std::string(""), "Path to the label text file");
    parser.addOption("--model, --modelPath", std::string(""), "Path to the model file");
    parser.addOption("--image, --imagePath", std::string(""), "Path to the input image file");

    parser.addSubOption("objDetectParams", "--conf_threshold", float(0.25), "objDetectParams conf_threshold");
    parser.addSubOption("objDetectParams", "--nms_threshold", float(0.45), "objDetectParams nms_threshold");
    parser.addSubOption("objDetectParams", "--pads_left", int(0), "objDetectParams pads_left");
    parser.addSubOption("objDetectParams", "--pads_right", int(0), "objDetectParams pads_right");
    parser.addSubOption("objDetectParams", "--pads_top", int(0), "objDetectParams pads_top");
    parser.addSubOption("objDetectParams", "--pads_bottom", int(0), "objDetectParams pads_bottom");

    parser.setConfig("--cfg", "config.ini", "set an configuration ini file for all options");
    parser.parseArgs(argc, argv);


    ObjDetectApp app(std::move(parser));
    app.run();

    return 0;
}