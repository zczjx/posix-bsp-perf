#ifndef __MUXAPP_HPP__
#define __MUXAPP_HPP__

#include <framework/BasePerfCase.hpp>
#include <shared/BspLogger.hpp>
#include <shared/ArgParser.hpp>
#include <bsp_container/IMuxer.hpp>
#include <bsp_container/StreamReader.hpp>
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
        m_logger->setPattern();
        auto& params = getArgs();
        std::string muxerImpl;
        params.getOptionVal("--muxerImpl", muxerImpl);
        m_muxer = bsp_container::IMuxer::create(muxerImpl);
    }

    MuxApp(const MuxApp&) = delete;
    MuxApp& operator=(const MuxApp&) = delete;
    MuxApp(MuxApp&&) = delete;
    MuxApp& operator=(MuxApp&&) = delete;

    ~MuxApp()
    {
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, "MuxApp::~MuxApp()");
    }

private:
    void onInit() override
    {
        auto& params = getArgs();
        std::string outputVideoPath;
        params.getOptionVal("--outputVideoPath", outputVideoPath);

        IMuxer::MuxConfig muxConfig{true, 29.97};
        muxConfig.ts_recreate = params.getSubFlagVal("mux", "--ts_recreate");
        params.getSubOptionVal("mux", "--video_fps", muxConfig.video_fps);
        m_muxer->openContainerMux(outputVideoPath, muxConfig);

        std::string inputVideoPath;
        params.getOptionVal("--inputVideoPath", inputVideoPath);
        std::shared_ptr<StreamReader> strReader = m_muxer->getStreamReader(inputVideoPath);
        int stream_id = m_muxer->addStream(strReader);
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, "MuxApp::onInit() stream_id: {}", stream_id);
        m_streamReaderMap[stream_id] = strReader;
    }

    void onProcess() override
    {
        StreamPacket streamPacket;
        while (m_streamReaderMap[0]->readPacket(streamPacket) >= 0)
        {
            m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info,
                "MuxApp::onProcess() streamPacket: stream_index: {}, pts: {}, dts: {}, size: {}",
                streamPacket.stream_index, streamPacket.pts, streamPacket.dts, streamPacket.useful_pkt_size);
            m_muxer->writeStreamPacket(streamPacket);
        }
        m_muxer->endStreamMux();
    }

    void onRender() override
    {
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, "MuxApp::onRender()");
    }

    void onRelease() override
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        for (auto& reader_pair : m_streamReaderMap)
        {
            reader_pair.second->closeStreamReader();
        }
        m_muxer->closeContainerMux();
    }
private:
    std::string m_name {"[MuxerApp]:"};
    std::unique_ptr<bsp_perf::shared::BspLogger> m_logger{nullptr};
    std::unique_ptr<IMuxer> m_muxer{nullptr};
    std::unordered_map<int, std::shared_ptr<StreamReader>> m_streamReaderMap{};
};

} // namespace perf_cases
} // namespace bsp_perf
#endif // __MUXAPP_HPP__