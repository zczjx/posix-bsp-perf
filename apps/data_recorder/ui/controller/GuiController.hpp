#ifndef __GUI_CONTROLLER_HPP__
#define __GUI_CONTROLLER_HPP__

#include <QApplication>
#include <QObject>
#include <QImage>

#include <ui/view/VideoFrameWidget.h>
#include <ui/model/RawCamera.hpp>
#include <ui/model/ObjectsDetection.hpp>
#include <ui/model/Recorder.hpp>

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <thread>
#include <atomic>

using json = nlohmann::json;

namespace apps
{
namespace data_recorder
{
namespace ui
{

class GuiController : public QObject
{
    Q_OBJECT
public:
    GuiController(int argc, char *argv[], const json& gui_ipc);

    ~GuiController();

    void runLoop();

private:
    void setupConnections();

    int updateFrameRecord(uint8_t* data, int width, int height);

private slots:
    void onRawCameraFrameUpdated(uint8_t* data, int width, int height);

    void onObjectsDetectionFrameUpdated(uint8_t* data, int width, int height);

    void onRecordStatusChanged(bool on);

private:
    // view
    std::unique_ptr<QApplication> m_app;
    std::unique_ptr<VideoFrameWidget> m_video_frame_widget;

    // model
    std::unique_ptr<RawCamera> m_raw_camera;
    std::unique_ptr<ObjectsDetection> m_objects_detection;
    std::unique_ptr<Recorder> m_recorder;

    bool m_record_enabled{false};
    std::mutex m_record_enabled_mutex;

};

} // namespace ui
} // namespace data_recorder
} // namespace apps
#endif // __GUI_CONTROLLER_HPP__