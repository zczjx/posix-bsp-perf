#include <iostream>
#include <fstream>
#include <string>
#include <shared/ArgParser.hpp>
#include <nlohmann/json.hpp>
#include "controller/GuiController.hpp"

using namespace apps::data_recorder::ui;
using namespace bsp_perf::shared;
using json = nlohmann::json;

int main(int argc, char *argv[])
{
    ArgParser parser("DataRecorderUI");
    parser.addOption("--nodes_ipc", "nodes_ipc.json", "path to the sensor ipc file");
    parser.addOption("--encoder, --encoderType", std::string("rkmpp"), "decoder type: rkmpp");
    parser.addOption("--g2d, --graphics2D", std::string("rkrga"), "graphics 2d platform type: rkrga");
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

    GuiController gui_controller(argc, argv, nodes_ipc);
    gui_controller.runLoop();

    return 0;
}