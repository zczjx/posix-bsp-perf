#ifndef __DEMUXAPP_HPP__
#define __DEMUXAPP_HPP__

#include <framework/BasePerfCase.hpp>
#include <shared/BspLogger.hpp>
#include <shared/ArgParser.hpp>
#include <bsp_container/IDemuxer.hpp>
#include <memory>
#include <string>
#include <iostream>
#include <vector>

namespace bsp_perf {
namespace perf_cases {
using namespace bsp_container;

class DemuxApp : public bsp_perf::common::BasePerfCase
{
public:
    DemuxApp(bsp_perf::shared::ArgParser&& args):
        BasePerfCase(std::move(args)),
        m_logger{std::make_unique<bsp_perf::shared::BspLogger>("DemuxApp")}
    {
        m_logger->setPattern();
        auto& params = getArgs();
        std::string demuxerImpl;
        std::string pluginPath;
        std::string labelTextPath;
        params.getOptionVal("--demuxerImpl", demuxerImpl);
        m_demuxer = bsp_container::IDemuxer::create(demuxerImpl);
    }

    DemuxApp(const DemuxApp&) = delete;
    DemuxApp& operator=(const DemuxApp&) = delete;
    DemuxApp(DemuxApp&&) = delete;
    DemuxApp& operator=(DemuxApp&&) = delete;
    ~DemuxApp()
    {
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, "DemuxApp::~DemuxApp()");
    }

private:

    void onInit() override
    {
        auto& params = getArgs();
        std::string inputVideoPath;
        params.getOptionVal("--inputVideoPath", inputVideoPath);
        m_demuxer->openContainerDemux(inputVideoPath);

        ContainerInfo containerInfo;
        m_demuxer->getContainerInfo(containerInfo);
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, "DemuxApp::onInit() containerInfo: container_type: {}, num_of_streams: {}",
            containerInfo.container_type, containerInfo.num_of_streams);

        for (const auto& metadata : containerInfo.metadata)
        {
            m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info,
                "DemuxApp::onInit() metadata: key: {}, value: {}",
                metadata.first, metadata.second);
        }
    }

    void onProcess() override
    {
        StreamPacket streamPacket;
        while(m_demuxer->readStreamPacket(streamPacket) >= 0)
        {
            m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info,
                "DemuxApp::onProcess() streamPacket: stream_index: {}, pts: {}, dts: {}, size: {}",
                streamPacket.stream_index, streamPacket.pts, streamPacket.dts, streamPacket.pkt_size);
        }
    }

    void onRender() override
    {
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, "DemuxApp::onRender()");
    }

    void onRelease() override
    {
        m_demuxer.reset();
    }


private:
    std::string m_name {"[DemuxApp]:"};
    std::unique_ptr<bsp_perf::shared::BspLogger> m_logger{nullptr};
    std::unique_ptr<IDemuxer> m_demuxer{nullptr};
};
} // namespace perf_cases
} // namespace bsp_perf

#endif // __DEMUXAPP_HPP__