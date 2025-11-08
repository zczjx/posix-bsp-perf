#include <iostream>
#include <string>
#include "DecodeApp.hpp"
#include "EncodeApp.hpp"

using namespace bsp_perf::perf_cases;
using namespace bsp_perf::shared;

int main(int argc, char* argv[])
{
    ArgParser parser("video_dec_enc");
    parser.addOption("--model", std::string(""), "decode | encode");

    // Decoder options
    parser.addOption("--input, --inputVideoPath", std::string(""), "Path to the input video file (H264/H265)");
    parser.addOption("--output, --outputVideoPath", std::string("./output.yuv"), "Path to the output YUV file");
    parser.addOption("--decoder, --decoderImpl", std::string("rkmpp"), "Decoder implementation: rkmpp | nvdec");
    parser.addOption("--encoder, --encoderImpl", std::string("rkmpp"), "Encoder implementation: rkmpp | nvenc");
    parser.addOption("--encodingType", std::string("h264"), "Encoding type: h264 | h265");
    parser.addOption("--fps", int(30), "Frame rate");
    parser.addOption("--frameFormat", std::string("YUV420SP"), "Frame format");
    parser.addOption("--width", uint32_t(1920), "Video width");
    parser.addOption("--height", uint32_t(1080), "Video height");

    parser.setConfig("--cfg", "config.ini", "set an configuration ini file for all options");
    parser.parseArgs(argc, argv);

    std::string model;
    parser.getOptionVal("--model", model);
    std::cout << "model: " << model << std::endl;

    if (model == "decode")
    {
        DecodeApp app(std::move(parser));
        app.run();
    }
    else if (model == "encode")
    {
        EncodeApp app(std::move(parser));
        app.run();
    }
    else
    {
        std::cout << "Invalid model: " << model << std::endl;
        std::cout << "Usage: " << argv[0] << " --model [decode|encode]" << std::endl;
    }

    return 0;
}

