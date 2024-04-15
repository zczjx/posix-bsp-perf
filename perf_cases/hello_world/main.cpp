#include <iostream>
#include "HelloPerf.hpp"

using namespace bsp_perf::perf_cases;

int main(int argc, char* argv[])
{
    HelloPerf hello;
    hello.run();

    return 0;
}