
#ifndef __CAMERA_CLIENT_HPP__
#define __CAMERA_CLIENT_HPP__

#include <sensor_clients/SensorClient.hpp>
#include <protocol/RtpReader.hpp>
#include <protocol/RtpHeader.hpp>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <VideoDecHelper.hpp>
using namespace bsp_perf::protocol;
namespace apps
{
namespace data_recorder
{

class CameraClient : public SensorClient
{
public:
    explicit CameraClient(const json& sensor_context, const json& vehicle_info);

    virtual ~CameraClient();

    std::shared_ptr<DecodeOutFrame> getCameraVideoFrame();

private:

    void runLoop() override;

private:
    int m_xres;
    int m_yres;
    std::string m_raw_pixel_format;
    std::unique_ptr<std::thread> m_main_thread{nullptr};
    std::atomic<bool> m_stopSignal{false};
    RtpReader m_rtp_reader;
    RtpHeader m_rtp_info{};
    std::mutex m_rtp_info_mutex;
    std::unique_ptr<VideoDecHelper> m_video_dec_helper;
};

} // namespace data_recorder
} // namespace apps
#endif // __CAMERA_CLIENT_HPP__