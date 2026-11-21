#include "SvsNode.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <fstream>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <profiler/BspTrace.hpp>
#include <shared/ArgParser.hpp>
#include <stdexcept>
#include <string>
#include <thread>

namespace
{
using json = nlohmann::json;

// Ctrl+C / kill 时让 runLoop() 正常退出，确保 BspTrace 析构并 flush trace 文件
std::atomic<bool> g_stopRequested{false};

void onSignal(int)
{
    g_stopRequested.store(true);
}

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
    parser.addOption("--trace_file", std::string(""),
                     "write a perfetto trace to this file (empty = tracing disabled)");
    parser.parseArgs(argc, argv);

    std::string nodesIpcPath;
    std::string rigPath;
    std::string dataRoot;
    std::string traceFile;
    parser.getOptionVal("--nodes_ipc", nodesIpcPath);
    parser.getOptionVal("--rig", rigPath);
    parser.getOptionVal("--data_root", dataRoot);
    parser.getOptionVal("--trace_file", traceFile);

    // 仅在指定 --trace_file 时才启用 perfetto，默认不影响正常运行
    std::unique_ptr<bsp_perf::common::BspTrace> trace;
    if (!traceFile.empty()) {
        trace = std::make_unique<bsp_perf::common::BspTrace>(traceFile);
    }

    try {
        const json nodesIpc = loadJson(nodesIpcPath);
        const json rigDocument = loadJson(rigPath);
        if (!rigDocument.contains("rig") || !rigDocument["rig"].is_object()) {
            throw std::runtime_error("vehicle rig file missing rig object");
        }

        apps::data_recorder::SvsNode node(nodesIpc, rigDocument["rig"], dataRoot);

        std::signal(SIGINT, onSignal);
        std::signal(SIGTERM, onSignal);
        std::thread watcher([&node]() {
            while (!g_stopRequested.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            node.stop();
        });
        node.runLoop();
        g_stopRequested.store(true);
        watcher.join();
    } catch (const std::exception& e) {
        std::cerr << "SvsNode failed: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}
