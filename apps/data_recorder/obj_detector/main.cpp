#include <iostream>
#include <fstream>
#include <string>
#include <shared/ArgParser.hpp>
#include <nlohmann/json.hpp>
#include "objDetector.hpp"

using namespace apps::data_recorder;
using namespace bsp_perf::shared;
using json = nlohmann::json;

int main(int argc, char *argv[])
{
    ArgParser parser("ObjDetector");
    parser.addOption("--nodes_ipc", "nodes_ipc.json", "path to the sensor ipc file");
    parser.addOption("--dnn, --dnnType", std::string("rknn"), "DNN type: trt or rknn");
    parser.addOption("--plugin, --pluginPath", std::string(""), "Path to the plugin library");
    parser.addOption("--label, --labelTextPath", std::string(""), "Path to the label text file");
    parser.addOption("--model, --modelPath", std::string(""), "Path to the model file");

    parser.addSubOption("objDetectParams", "--conf_threshold", float(0.25), "objDetectParams conf_threshold");
    parser.addSubOption("objDetectParams", "--nms_threshold", float(0.45), "objDetectParams nms_threshold");
    parser.addSubOption("objDetectParams", "--pads_left", int(0), "objDetectParams pads_left");
    parser.addSubOption("objDetectParams", "--pads_right", int(0), "objDetectParams pads_right");
    parser.addSubOption("objDetectParams", "--pads_top", int(0), "objDetectParams pads_top");
    parser.addSubOption("objDetectParams", "--pads_bottom", int(0), "objDetectParams pads_bottom");

    parser.setConfig("--cfg", "config.ini", "set an configuration ini file for all options");
    parser.parseArgs(argc, argv);

    std::string nodes_ipc_file;
    parser.getOptionVal("--nodes_ipc", nodes_ipc_file);
    json nodes_ipc{};
    try
    {
        nodes_ipc = json::parse(std::ifstream(nodes_ipc_file));
    }
    catch (const json::parse_error& e)
    {
        std::cerr << "Failed to parse nodes ipc file: " << e.what() << std::endl;
        return -1;
    }

    ObjDetector obj_detector(std::move(parser), nodes_ipc);
    obj_detector.runLoop();

    return 0;
}