#ifndef __GUI_CONTROLLER_HPP__
#define __GUI_CONTROLLER_HPP__

#include <QApplication>
#include <QElapsedTimer>
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

    int updateFrameRecord();

private slots:
    void onRawCameraFrameUpdated(const QString& sensorName, uint8_t* data, int width, int height, const QString& format);

    void onObjectsDetectionFrameUpdated(const QString& detectorName, uint8_t* data, int width, int height, const QString& format);

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
    QElapsedTimer m_record_frame_timer;
    int m_record_interval_ms{33};

};

} // namespace ui
} // namespace data_recorder
} // namespace apps
#endif // __GUI_CONTROLLER_HPP__