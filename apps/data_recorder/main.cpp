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

    std::ifstream rig_file(rig_file);
    json rig_json;

    try
    {
        rig_json = json::parse(rig_file, nullptr, true, true);
    }
    catch (const json::parse_error& e)
    {
        std::cerr << "Failed to parse rig file: " << e.what() << std::endl;
        return -1;
    }

    CarlaVehicle vehicle(rig_json["rig"], output_file);
    vehicle.run();

    return 0;
}