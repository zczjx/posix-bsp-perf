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
    parser.addOption("--label, --labelTextPath", std::string(""), "Path to the label text file");
    parser.addOption("--model, --modelPath", std::string(""), "Path to the model file");
    parser.addOption("--image, --imagePath", std::string(""), "Path to the input image file");
    parser.setConfig("--cfg", "config.ini", "set an configuration ini file for all options");
    parser.parseArgs(argc, argv);


    ObjDetectApp app(std::move(parser));
    app.run();

    return 0;
}