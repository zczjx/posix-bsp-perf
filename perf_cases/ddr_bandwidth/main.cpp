#include <iostream>
#include <string>
#include "ddrPerf.hpp"

using namespace bsp_perf::perf_cases;
using namespace bsp_perf::shared;
using namespace std::string_literals;

int main(int argc, char* argv[])
{
    ArgParser parser("ddrPerf");
    parser.addOption("--case_name", "DDR RW Bandwidth"s, "name of perf test case");
    parser.addOption("--profile_path", "logs/ddr_rw.profile"s, "path the of the profile file");
    parser.addOption("--size_mb", size_t(256), "Memory size for the perf case");
    parser.addOption("--cycles", int32_t(5), "Running cycles for the perf case");
    parser.parseArgs(argc, argv);

    int32_t cycles;
    parser.getOptionVal("--cycles", cycles);

    ddrPerf perf_case(std::move(parser));
    perf_case.run(cycles);

    return 0;
}