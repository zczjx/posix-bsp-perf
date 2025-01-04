#include <iostream>
#include <string>
#include "HelloPerf.hpp"

using namespace bsp_perf::perf_cases;
using namespace bsp_perf::shared;

int main(int argc, char* argv[])
{
    ArgParser parser("HelloPerf");
    parser.addOption("-a,--append", std::string("zczjx default append"), "append string to print in Hello World");
    parser.addFlag("-e,--enable", false, "enable flag to print Hello World");
    parser.addOption("--cycles", int32_t(5), "Running cycles for the perf case");
    parser.setConfig("--cfg", "config.ini", "set an configuration ini file for all options");
    parser.addOption("-l,--list", std::string("aaa;bbb;ccc"), "using ';' to split the string list");
    parser.parseArgs(argc, argv);

    int32_t cycles;
    parser.getOptionVal("--cycles", cycles);

    HelloPerf hello(std::move(parser));
    hello.run(cycles);

    return 0;
}