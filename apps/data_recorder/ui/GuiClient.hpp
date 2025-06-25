#ifndef __GUI_CLIENT_HPP__
#define __GUI_CLIENT_HPP__

#include <QApplication>
#include "VideoFrameWidget.h"
#include <zeromq_ipc/sharedMemSubscriber.hpp>

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <thread>
#include <atomic>

using json = nlohmann::json;
using namespace midware::zeromq_ipc;
namespace apps
{
namespace data_recorder
{
namespace ui
{

class GuiClient
{
public:
    GuiClient(int argc, char *argv[], const json& gui_ipc);
    ~GuiClient();

    void runLoop();

private:
    void installGuiUpdateCallback();

    void CameraConsumerLoop(std::shared_ptr<SharedMemSubscriber> input_shmem_port);

private:
    std::unique_ptr<QApplication> m_app;
    std::unique_ptr<VideoFrameWidget> m_video_frame_widget;

    // the key of the map is the sensor name
    // the value of the map is (sensor type, shm_subscriber)
    std::unordered_map<std::string, std::pair<std::string, std::shared_ptr<SharedMemSubscriber>>> m_input_shmem_ports;
    std::vector<std::thread> m_input_shmem_threads;

    std::atomic<bool> m_stopSignal{false};

};

} // namespace ui
} // namespace data_recorder
} // namespace apps


#endif // __GUI_CLIENT_HPP__
