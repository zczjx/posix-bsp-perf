#include "SvsNode.hpp"

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <shared/ArgParser.hpp>
#include <stdexcept>
#include <string>

namespace
{
using json = nlohmann::json;

json loadJson(const std::string& path)
{
    std::ifstream stream(path);
    if (!stream.is_open()) {
        throw std::runtime_error("failed to open JSON file: " + path);
    }

    json result;
    stream >> result;
    return result;
}
} // namespace

int main(int argc, char* argv[])
{
    bsp_perf::shared::ArgParser parser("SvsNode");
    parser.addOption("--nodes_ipc", "nodes_ipc.json", "path to the nodes IPC file");
    parser.addOption("--rig", "vehicle_rig.json", "path to the vehicle rig file");
    parser.addOption("--data_root", std::string(""), "base path for optional SVS assets");
    parser.parseArgs(argc, argv);

    std::string nodesIpcPath;
    std::string rigPath;
    std::string dataRoot;
    parser.getOptionVal("--nodes_ipc", nodesIpcPath);
    parser.getOptionVal("--rig", rigPath);
    parser.getOptionVal("--data_root", dataRoot);

    try {
        const json nodesIpc = loadJson(nodesIpcPath);
        const json rigDocument = loadJson(rigPath);
        if (!rigDocument.contains("rig") || !rigDocument["rig"].is_object()) {
            throw std::runtime_error("vehicle rig file missing rig object");
        }

        apps::data_recorder::SvsNode node(nodesIpc, rigDocument["rig"], dataRoot);
        node.runLoop();
    } catch (const std::exception& e) {
        std::cerr << "SvsNode failed: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}
