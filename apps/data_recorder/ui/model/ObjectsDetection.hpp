#ifndef __OBJECTS_DETECTION_HPP__
#define __OBJECTS_DETECTION_HPP__

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
class ObjectsDetection : public QObject
{
    Q_OBJECT
public:
    ObjectsDetection(const json& gui_ipc);
    ~ObjectsDetection();

private:
    void setupObjectDetectionConsumer(const json& gui_ipc);

    void ObjectDetectionConsumerLoop(std::shared_ptr<SharedMemSubscriber> input_shmem_port);

signals:
    void objectsDetectionFrameUpdated(uint8_t* data, int width, int height, const QString& format);

private:
    std::unordered_map<std::string, std::pair<std::string, std::shared_ptr<SharedMemSubscriber>>> m_input_shmem_ports;
    std::vector<std::thread> m_input_shmem_threads;
    std::atomic<bool> m_stopSignal{false};
};

} // namespace ui
} // namespace data_recorder
} // namespace apps

#endif // __OBJECTS_DETECTION_HPP__