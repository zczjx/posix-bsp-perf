#include <iostream>
#include "HelloPerf.hpp"

using namespace bsp_perf::perf_cases;

int main(int argc, char* argv[])
{
    HelloPerf hello;
    hello.setUp();
    hello.run();
    hello.tearDown();

    return 0;
}