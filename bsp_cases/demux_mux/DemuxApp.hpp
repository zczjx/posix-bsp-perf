#ifndef __DEMUXAPP_HPP__
#define __DEMUXAPP_HPP__

#include <framework/BasePerfCase.hpp>
#include <shared/BspLogger.hpp>
#include <shared/ArgParser.hpp>
#include <bsp_container/IDemuxer.hpp>
#include <shared/BspFileUtils.hpp>
#include <bsp_container/StreamWriter.hpp>
#include <memory>
#include <string>
#include <iostream>
#include <vector>
#include <unordered_map>

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
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info,
            "DemuxApp::onInit() containerInfo: container_type: {}, num_of_streams: {}, bit_rate: {}, packet_size: {}",
            containerInfo.container_type, containerInfo.num_of_streams, containerInfo.bit_rate, containerInfo.packet_size);

        for (const auto& metadata : containerInfo.metadata)
        {
            m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info,
                "DemuxApp::onInit() metadata: key: {}, value: {}",
                metadata.first, metadata.second);
        }

        for (const auto& stream_item : containerInfo.stream_info_list)
        {
            m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info,
                "DemuxApp::onInit() stream_info: index: {}, start_time: {}, duration: {}, num_of_frames: {}",
                stream_item.index, stream_item.start_time, stream_item.duration, stream_item.num_of_frames);

            for (const auto& metadata : stream_item.metadata)
            {
                m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info,
                    "DemuxApp::onInit() stream_metadata: key: {}, value: {}",
                    metadata.first, metadata.second);
            }

            for (const auto& disposition : stream_item.disposition_list)
            {
                m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info,
                    "DemuxApp::onInit() stream_disposition: {}",
                    disposition);
            }

            m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info,
                "DemuxApp::onInit() stream_codec_params: codec_type: {}, codec_name: {}, bit_rate: {}, sample_aspect_ratio: {}, frame_rate: {}, width: {}, height: {}, sample_rate: {}",
                stream_item.codec_params.codec_type, stream_item.codec_params.codec_name, stream_item.codec_params.bit_rate,
                stream_item.codec_params.sample_aspect_ratio, stream_item.codec_params.frame_rate, stream_item.codec_params.width,
                stream_item.codec_params.height, stream_item.codec_params.sample_rate);
            m_streamInfoMap[stream_item.index] = stream_item;
            std::string outputVideoPath;
            params.getOptionVal("--outputVideoPath", outputVideoPath);
            m_streamWriterMap[stream_item.index] = m_demuxer->getStreamWriter(stream_item.index,  std::to_string(stream_item.index) + "_" + outputVideoPath);
        }
    }

    void onProcess() override
    {
        StreamPacket streamPacket;
        while(m_demuxer->readStreamPacket(streamPacket) >= 0)
        {
            m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info,
                "DemuxApp::onProcess() streamPacket: stream_index: {}, pts: {}, dts: {}, size: {}",
                streamPacket.stream_index, streamPacket.pts, streamPacket.dts, streamPacket.useful_pkt_size);

            std::string codec_type = m_streamInfoMap[streamPacket.stream_index].codec_params.codec_type;
            if (codec_type == "video")
            {
                m_frame_count++;
                if(1 == m_frame_count)
                {
                    m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, "DemuxApp::onProcess() write h264 header");
                    m_streamWriterMap[streamPacket.stream_index]->writeHeader();
                }
                m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info,
                    "DemuxApp::onProcess() video streamPacket: stream_index: {}, pts: {}, dts: {}, duration: {}, size: {}",
                    streamPacket.stream_index, streamPacket.pts, streamPacket.dts, streamPacket.duration, streamPacket.useful_pkt_size);
                m_streamWriterMap[streamPacket.stream_index]->writePacket(streamPacket);
            }
        }
        for (auto& writer_pair : m_streamWriterMap)
        {
            writer_pair.second->writeTrailer();
        }
    }

    void onRender() override
    {
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, "DemuxApp::onRender()");
    }

    void onRelease() override
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        for (auto& writer_pair : m_streamWriterMap)
        {
            writer_pair.second->closeStreamWriter();
        }
        m_demuxer.reset();
    }

private:
    std::string m_name {"[DemuxApp]:"};
    std::unique_ptr<bsp_perf::shared::BspLogger> m_logger{nullptr};
    std::unique_ptr<IDemuxer> m_demuxer{nullptr};
    std::unordered_map<int, StreamInfo> m_streamInfoMap{};
    std::unordered_map<int, std::shared_ptr<StreamWriter>> m_streamWriterMap{};
    size_t m_frame_count{0};
};
} // namespace perf_cases
} // namespace bsp_perf

#endif // __DEMUXAPP_HPP__