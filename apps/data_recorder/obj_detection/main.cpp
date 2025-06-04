#include <iostream>
#include <fstream>
#include <string>
#include <shared/ArgParser.hpp>
#include <nlohmann/json.hpp>

using namespace apps::data_recorder;
using namespace bsp_perf::shared;
using json = nlohmann::json;

int main(int argc, char *argv[])
{
    ArgParser parser("DataRecorderObjDetection");
    parser.addOption("--input_ipc", "sensor_ipc.json", "path to the sensor ipc file");
    parser.addOption("--output_ipc", "obj_detection.json", "path to the obj detection ipc file");
    parser.addOption("--g2d, --graphics2D", std::string("rkrga"), "graphics 2d platform type: rkrga");
    parser.parseArgs(argc, argv);

    std::string input_ipc_file;
    parser.getOptionVal("--input_ipc", input_ipc_file);
    json input_ipc{};
    try
    {
        input_ipc = json::parse(std::ifstream(input_ipc_file));
    }
    catch (const json::parse_error& e)
    {
        std::cerr << "Failed to parse sensor ipc file: " << e.what() << std::endl;
        return -1;
    }

    std::string output_ipc_file;
    parser.getOptionVal("--output_ipc", output_ipc_file);
    json output_ipc{};
    try
    {
        output_ipc = json::parse(std::ifstream(output_ipc_file));
    }
    catch (const json::parse_error& e)
    {
        std::cerr << "Failed to parse obj detection ipc file: " << e.what() << std::endl;
        return -1;
    }

    std::string g2dPlatform;
    parser.getOptionVal("--graphics2D", g2dPlatform);

    GuiClient gui_client(argc, argv, sensor_ipc, g2dPlatform);
    gui_client.runLoop();

    return 0;
}