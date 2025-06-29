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
    ArgParser parser("DataRecorderObjDetection");
    parser.addOption("--nodes_ipc", "nodes_ipc.json", "path to the sensor ipc file");
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

    ObjDetector obj_detector(nodes_ipc);
    obj_detector.runLoop();

    return 0;
}