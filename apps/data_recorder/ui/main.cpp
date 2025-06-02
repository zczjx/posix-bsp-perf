#include <iostream>
#include <fstream>
#include <string>
#include <shared/ArgParser.hpp>
#include <nlohmann/json.hpp>

#include "GuiClient.hpp"

using namespace apps::data_recorder::ui;
using namespace bsp_perf::shared;
using json = nlohmann::json;

int main(int argc, char *argv[])
{
    ArgParser parser("DataRecorderUI");
    parser.addOption("--sensor_ipc", "sensor_ipc.json", "path to the sensor ipc file");
    parser.addOption("--g2d, --graphics2D", std::string("rkrga"), "graphics 2d platform type: rkrga");
    parser.parseArgs(argc, argv);

    std::string sensor_ipc_file;
    parser.getOptionVal("--sensor_ipc", sensor_ipc_file);
    json sensor_ipc{};
    try
    {
        sensor_ipc = json::parse(std::ifstream(sensor_ipc_file));
    }
    catch (const json::parse_error& e)
    {
        std::cerr << "Failed to parse sensor ipc file: " << e.what() << std::endl;
        return -1;
    }

    std::string g2dPlatform;
    parser.getOptionVal("--graphics2D", g2dPlatform);

    GuiClient gui_client(argc, argv, sensor_ipc, g2dPlatform);
    gui_client.runLoop();

    return 0;
}