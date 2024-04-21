#include "BasePerfCase.hpp"
#include<iostream>


namespace bsp_perf {
namespace common {

constexpr char BasePerfCase::LOG_TAG[];

BasePerfCase::BasePerfCase()
{
    std::cout << LOG_TAG << "::BasePerfCase()" << std::endl;
}

void BasePerfCase::run()
{
    std::cout << LOG_TAG << "BasePerfCase::run()" << std::endl;
    onInit();
    onProcess();
    onPerfPrint();
    onRender();
    onRelease();
}


void BasePerfCase::onInit()
{
    std::cout << LOG_TAG << "BasePerfCase::onInit()" << std::endl;
}

void BasePerfCase::onProcess()
{
    std::cout << LOG_TAG << "BasePerfCase::onProcess()" << std::endl;
}

void BasePerfCase::onRender()
{
    std::cout << LOG_TAG << "BasePerfCase::onRender()" << std::endl;
}

void BasePerfCase::onPerfPrint()
{
    std::cout << LOG_TAG << "BasePerfCase::onPerfPrint()" << std::endl;
}

void BasePerfCase::onRelease()
{
    std::cout << LOG_TAG << "BasePerfCase::onRelease()" << std::endl;
}

} // namespace common
} // namespace bsp_perf
