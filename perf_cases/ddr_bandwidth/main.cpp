#include <iostream>
#include <string>
#include "ddrPerf.hpp"

using namespace bsp_perf::perf_cases;
using namespace bsp_perf::shared;

int main(int argc, char* argv[])
{
    ArgParser parser("ddrPerf");
    parser.addOption("--case_name", std::string("DDR RW Bandwidth"), "name of perf test case");
    parser.addOption("--size_mb", size_t(256), "Memory size for the perf case");
    parser.parseArgs(argc, argv);

    ddrPerf perf_case(std::move(parser));
    perf_case.run(5);

    return 0;
}