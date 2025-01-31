#include <iostream>
#include <string>
#include "DemuxApp.hpp"
#include "MuxApp.hpp"

using namespace bsp_perf::perf_cases;
using namespace bsp_perf::shared;

int main(int argc, char* argv[])
{
    ArgParser parser("demux_mux");
    parser.addOption("--model", std::string(""), "demux | mux");
    parser.addOption("--input, --inputVideoPath", std::string(""), "Path to the input video file");
    parser.addOption("--output, --outputVideoPath", std::string("./"), "Path to the output video file");
    parser.addOption("--demuxer, --demuxerImpl", std::string("FFmpegDemuxer"), "demuxer Impl: FFmpegDemuxer");
    parser.addOption("--muxer, --muxerImpl", std::string("FFmpegMuxer"), "Muxer Impl: FFmpegMuxer");
    parser.setConfig("--cfg", "config.ini", "set an configuration ini file for all options");
    parser.parseArgs(argc, argv);

    std::string model;
    parser.getOptionVal("--model", model);

    if (model == "demux")
    {
        DemuxApp app(std::move(parser));
        app.run();
    }
    else if (model == "mux")
    {
        MuxApp app(std::move(parser));
        app.run();
    }
    else
    {
        std::cout << "Invalid model: " << model << std::endl;
    }

    return 0;
}