#include "BasePerfCase.hpp"
#include<iostream>


namespace bsp_perf {
namespace common {

void BasePerfCase::setUp() {
    std::cout << "BasePerfCase::setUp()" << std::endl;
}

void BasePerfCase::run() {
    std::cout << "BasePerfCase::run()" << std::endl;
    onInit();
    onProcess();
}

void BasePerfCase::tearDown() {
    std::cout << "BasePerfCase::tearDown()" << std::endl;

}

void BasePerfCase::onInit() {
    std::cout << "BasePerfCase::onInit()" << std::endl;
}

void BasePerfCase::onProcess() {
    std::cout << "BasePerfCase::onProcess()" << std::endl;
}

void BasePerfCase::onPerfPrint() {
    std::cout << "BasePerfCase::onPerfPrint()" << std::endl;
}

void BasePerfCase::onRelease() {
    std::cout << "BasePerfCase::onRelease()" << std::endl;
}

} // namespace common
} // namespace bsp_perf
