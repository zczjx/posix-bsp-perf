#include <iostream>
#include <string>
#include "HelloPerf.hpp"

using namespace bsp_perf::perf_cases;
using namespace bsp_perf::shared;

int main(int argc, char* argv[])
{
    ArgParser parser("HelloPerf");
    parser.addSubOption("subcmd1", "-a,--append", std::string("zczjx default append"), "append string to print in Hello World");
    parser.addSubFlag("subcmd1", "-e,--enable", false, "enable flag to print Hello World");
    parser.addSubOption("subcmd1", "--cycles", int32_t(5), "Running cycles for the perf case");
    parser.addSubOption("subcmd1", "-l,--list", std::string("aaa;bbb;ccc"), "using ';' to split the string list");

    parser.addSubOption("subcmd2", "-a,--append", std::string("zczjx default append"), "append string to print in Hello World");
    parser.addSubFlag("subcmd2", "-e,--enable", false, "enable flag to print Hello World");
    parser.addSubOption("subcmd2", "--cycles", int32_t(5), "Running cycles for the perf case");
    parser.addSubOption("subcmd2", "-l,--list", std::string("aaa;bbb;ccc"), "using ';' to split the string list");

    parser.addSubOption("subcmd3", "-a,--append", std::string("zczjx default append"), "append string to print in Hello World");
    parser.addSubFlag("subcmd3", "-e,--enable", false, "enable flag to print Hello World");
    parser.addSubOption("subcmd3", "--cycles", int32_t(5), "Running cycles for the perf case");
    parser.addSubOption("subcmd3", "-l,--list", std::string("aaa;bbb;ccc"), "using ';' to split the string list");

    parser.setConfig("--cfg", "config.ini", "set an configuration ini file for all options");
    parser.parseArgs(argc, argv);

    int32_t cycles;
    parser.getSubOptionVal("subcmd2", "--cycles", cycles);

    HelloPerf hello(std::move(parser));
    hello.run(cycles);

    return 0;
}