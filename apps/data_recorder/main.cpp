#include "CarlaVehicle.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <shared/ArgParser.hpp>
#include <nlohmann/json.hpp>

using namespace apps::data_recorder;
using namespace bsp_perf::shared;
using json = nlohmann::json;

int main(int argc, char* argv[])
{
    ArgParser parser("DataRecorder");
    parser.addOption("--rig", "carla-vehicle-view.json", "path to the rig file");
    parser.addOption("--output", "data_recorder.mp4", "path to the output file");
    parser.parseArgs(argc, argv);

    std::string rig_file;
    parser.getOptionVal("--rig", rig_file);

    std::string output_file;
    parser.getOptionVal("--output", output_file);
    json json_data{};

    try
    {
        json_data = json::parse(std::ifstream(rig_file));
    }
    catch (const json::parse_error& e)
    {
        std::cerr << "Failed to parse rig file: " << e.what() << std::endl;
        return -1;
    }

    CarlaVehicle vehicle(json_data["rig"], output_file);
    vehicle.run();

    return 0;
}