
#ifndef __CAMERA_CLIENT_HPP__
#define __CAMERA_CLIENT_HPP__

#include <sensor_clients/SensorClient.hpp>
#include <protocol/RtpReader.hpp>
#include <protocol/RtpHeader.hpp>
#include <zeromq_ipc/sharedMemPublisher.hpp>
#include <zeromq_ipc/zmqSubscriber.hpp>
#include <common/msg/CameraSensorMsg.hpp>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include "VideoDecHelper.hpp"

using namespace bsp_perf::protocol;
namespace apps
{
namespace data_recorder
{
class CameraClient : public SensorClient
{
public:
    explicit CameraClient(const json& sensor_context, const json& vehicle_info, const json& node_ipc);

    virtual ~CameraClient();

    std::shared_ptr<DecodeOutFrame> getCameraVideoFrame();

private:

    void runLoop() override;

    void consumerLoop();

    void setupInputConfig(const json& sensor_context, const json& vehicle_info);

    void setupOutputConfig(const json& node_ipc);

    void fillCameraSensorMsg(CameraSensorMsg& msg, size_t data_size, size_t slot_index);

private:
    int m_xres;
    int m_yres;
    std::string m_raw_pixel_format;
    std::string m_out_pixel_format;
    std::string m_target_platform;
    std::unique_ptr<std::thread> m_main_thread{nullptr};
    std::atomic<bool> m_stopSignal{false};
    RtpReader m_rtp_reader;
    RtpHeader m_rtp_info{};
    std::mutex m_rtp_info_mutex;
    std::unique_ptr<VideoDecHelper> m_video_dec_helper;

    std::unique_ptr<std::thread> m_consumer_thread{nullptr};

    std::shared_ptr<ZmqSubscriber> m_input_port{nullptr};
    std::shared_ptr<SharedMemPublisher> m_output_shmem_port{nullptr};
};

} // namespace data_recorder
} // namespace apps
#endif // __CAMERA_CLIENT_HPP__