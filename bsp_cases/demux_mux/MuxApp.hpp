#ifndef __MUXAPP_HPP__
#define __MUXAPP_HPP__

#include <framework/BasePerfCase.hpp>
#include <shared/BspLogger.hpp>
#include <shared/ArgParser.hpp>
#include <bsp_container/IMuxer.hpp>
#include <memory>
#include <string>
#include <iostream>
#include <vector>

namespace bsp_perf {
namespace perf_cases {
using namespace bsp_container;

class MuxApp: public bsp_perf::common::BasePerfCase
{
public:
    MuxApp(bsp_perf::shared::ArgParser&& args):
        BasePerfCase(std::move(args)),
        m_logger{std::make_unique<bsp_perf::shared::BspLogger>("MuxApp")}
    {
        ;
    }

    ~MuxApp();

    void initialize();
    void run();
    void shutdown();

private:
    void onInit() override
    {
        ;
    }
    void onProcess() override
    {
        ;
    }
    void onRender() override
    {
        ;
    }
    void onRelease() override
    {
        ;
    }

    void processInput();
    void multiplexData();
    void demultiplexData();

private:
    std::string m_name {"[MuxerApp]:"};
    std::unique_ptr<bsp_perf::shared::BspLogger> m_logger{nullptr};
    std::unique_ptr<IMuxer> m_muxer{nullptr};
};

} // namespace perf_cases
} // namespace bsp_perf
#endif // __MUXAPP_HPP__