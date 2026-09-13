#include "AppOptions.hpp"

#include <shared/ArgParser.hpp>

namespace apps::data_recorder::ui
{

namespace
{

#ifdef BUILD_PLATFORM_JETSON
constexpr const char* kDefaultEncoder = "nvenc";
constexpr const char* kDefaultG2d = "nvvic";
#else
constexpr const char* kDefaultEncoder = "rkmpp";
constexpr const char* kDefaultG2d = "rkrga";
#endif

} // namespace

bool AppOptions::parse(int argc, char* argv[], AppOptions& options)
{
    bsp_perf::shared::ArgParser parser("DataRecorderUI");
    parser.addOption("--nodes_ipc", std::string("nodes_ipc.json"),
        "path to the node ipc configuration file");
    parser.addOption("--encoder", std::string(kDefaultEncoder),
        "encoder platform type: rkmpp, nvenc");
    parser.addOption("--g2d", std::string(kDefaultG2d),
        "graphics 2d platform type: rkrga, nvvic");
    parser.addOption("--muxer", std::string("FFmpegMuxer"), "muxer implementation: FFmpegMuxer");
    parser.addOption("--output_dir", std::string(), "directory for recordings, default cwd");

    if (parser.parseArgs(argc, argv) != 0)
    {
        return false;
    }

    parser.getOptionVal("--nodes_ipc", options.nodesIpcFile);
    parser.getOptionVal("--encoder", options.encoderType);
    parser.getOptionVal("--g2d", options.g2dType);
    parser.getOptionVal("--muxer", options.muxerType);
    parser.getOptionVal("--output_dir", options.outputDir);

    return true;
}

} // namespace apps::data_recorder::ui
