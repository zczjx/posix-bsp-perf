#include <iostream>
#include <fstream>
#include <string>
#include <shared/ArgParser.hpp>
#include <nlohmann/json.hpp>

using namespace bsp_perf::shared;
using json = nlohmann::json;

int main(int argc, char* argv[])
{
    ArgParser parser("JsonParse");
    parser.addOption("--file", "json_file.json", "path to the json file");
    parser.parseArgs(argc, argv);

    std::string json_file;
    parser.getOptionVal("--file", json_file);

    std::ifstream file(json_file);
    if (!file.is_open())
    {
        std::cerr << "Failed to open json file" << std::endl;
        return -1;
    }

    json json_data;
    try
    {
        json_data = json::parse(file, nullptr, true, true);
    }
    catch (const json::parse_error& e)
    {
        std::cerr << "Failed to parse json file: " << e.what() << std::endl;
        return -1;
    }

    auto rig_object = json_data["rig"];

    auto vehicle_object = rig_object["vehicle"];

    std::string bp_id = vehicle_object["bp_id"];
    std::cout << "bp_id: " << bp_id << std::endl;

    std::string name = vehicle_object["name"];
    std::cout << "name: " << name << std::endl;

    auto sensors_array = rig_object["sensors"];
    for (auto& sensor_object : sensors_array)
    {
        std::string type = sensor_object["type"];
        std::cout << "type: " << type << std::endl;

        std::string bp_id = sensor_object["bp_id"];
        std::cout << "bp_id: " << bp_id << std::endl;

        std::string name = sensor_object["name"];
        std::cout << "name: " << name << std::endl;

        std::string status = sensor_object["status"];
        std::cout << "status: " << status << std::endl;

        auto spawn_point = sensor_object["spawn_point"];
        float x = spawn_point["x"];
        std::cout << "x: " << x << std::endl;

        float y = spawn_point["y"];
        std::cout << "y: " << y << std::endl;

        float z = spawn_point["z"];
        std::cout << "z: " << z << std::endl;

        float roll = spawn_point["roll"];
        std::cout << "roll: " << roll << std::endl;

        float pitch = spawn_point["pitch"];
        std::cout << "pitch: " << pitch << std::endl;

        int32_t image_size_x = sensor_object["image_size_x"];
        std::cout << "image_size_x: " << image_size_x << std::endl;

        int32_t image_size_y = sensor_object["image_size_y"];
        std::cout << "image_size_y: " << image_size_y << std::endl;

        float fov = sensor_object["fov"];
        std::cout << "fov: " << fov << std::endl;

        std::string encode_format = sensor_object["encode_format"];
        std::cout << "encode_format: " << encode_format << std::endl;

        std::string zmq_ipc = sensor_object["zmq_ipc"];
        std::cout << "zmq_ipc: " << zmq_ipc << std::endl;

        std::string zmq_transport = sensor_object["zmq_transport"];
        std::cout << "zmq_transport: " << zmq_transport << std::endl;

        std::string protocol = sensor_object["protocol"];
        std::cout << "protocol: " << protocol << std::endl;

        auto display_position_array = sensor_object["display_position"];
        for (auto& display_position : display_position_array)
        {
            std::cout << "num: " << display_position << std::endl;
        }

    }

    return 0;
}