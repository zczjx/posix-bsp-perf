#include <iostream>
#include <string>
#include "HelloPerf.hpp"

using namespace bsp_perf::perf_cases;
using namespace bsp_perf::shared;

int main(int argc, char* argv[])
{
    ArgParser parser("HelloPerf");
    parser.addOption("-a,--append", "append string to print in Hello World");
    parser.parseArgs(argc, argv);

    HelloPerf hello(std::move(parser));
    hello.run();

    return 0;
}