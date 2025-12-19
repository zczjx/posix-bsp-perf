#ifndef __RAW_CAMERA_HPP__
#define __RAW_CAMERA_HPP__

#include <zeromq_ipc/sharedMemSubscriber.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <thread>
#include <atomic>

#include <QObject>

using json = nlohmann::json;
using namespace midware::zeromq_ipc;
namespace apps
{
namespace data_recorder
{
namespace ui
{

class RawCamera : public QObject
{
    Q_OBJECT
public:
    RawCamera(const json& gui_ipc);
    ~RawCamera();

private:
    void setupCameraConsumer(const json& gui_ipc);

    void CameraConsumerLoop(std::shared_ptr<SharedMemSubscriber> input_shmem_port);

signals:
    void rawCameraFrameUpdated(uint8_t* data, int width, int height, const QString& format);

private:
    // the key of the map is the sensor name
    // the value of the map is (sensor type, shm_subscriber)
    std::unordered_map<std::string, std::pair<std::string, std::shared_ptr<SharedMemSubscriber>>> m_input_shmem_ports;
    std::vector<std::thread> m_input_shmem_threads;
    std::atomic<bool> m_stopSignal{false};
};

} // namespace ui
} // namespace data_recorder
} // namespace apps

#endif // __RAW_CAMERA_HPP__
