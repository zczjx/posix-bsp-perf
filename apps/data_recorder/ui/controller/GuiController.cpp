#include "GuiController.hpp"
#include <iostream>

namespace apps
{
namespace data_recorder
{
namespace ui
{

GuiController::GuiController(int argc, char *argv[], const json& gui_ipc)
{
    m_app = std::make_unique<QApplication>(argc, argv);
    m_video_frame_widget = std::make_unique<VideoFrameWidget>();
    m_video_frame_widget->show();

    m_raw_camera = std::make_unique<RawCamera>(gui_ipc);
    m_objects_detection = std::make_unique<ObjectsDetection>(gui_ipc);
    m_recorder = std::make_unique<Recorder>(argc, argv);
    setupConnections();
}

void GuiController::setupConnections()
{
    connect(m_raw_camera.get(), &RawCamera::rawCameraFrameUpdated, this, &GuiController::onRawCameraFrameUpdated);
    connect(m_objects_detection.get(), &ObjectsDetection::objectsDetectionFrameUpdated, this, &GuiController::onObjectsDetectionFrameUpdated);
    connect(m_video_frame_widget.get(), &VideoFrameWidget::recordStatusChanged, this, &GuiController::onRecordStatusChanged);
}

int GuiController::updateFrameRecord(uint8_t* data, int width, int height)
{
    if (m_record_enabled)
    {
        m_recorder->writeRecordFrame(data, width, height);
    }
    return 0;
}

GuiController::~GuiController()
{
    m_raw_camera.reset();
    m_objects_detection.reset();
    m_video_frame_widget.reset();
    m_app.reset();
}

void GuiController::runLoop()
{
    m_app->exec();
}

void GuiController::onRawCameraFrameUpdated(uint8_t* data, int width, int height)
{
    if (m_video_frame_widget->getCurrentDataSource() == VideoFrameWidget::RawCamera)
    {
        m_video_frame_widget->setFrame(data, width, height);
        updateFrameRecord(data, width, height);
    }
}

void GuiController::onObjectsDetectionFrameUpdated(uint8_t* data, int width, int height)
{
    if (m_video_frame_widget->getCurrentDataSource() == VideoFrameWidget::ObjectsDetection)
    {
        m_video_frame_widget->setFrame(data, width, height);
        updateFrameRecord(data, width, height);
    }
}

void GuiController::onRecordStatusChanged(bool on)
{
    std::lock_guard<std::mutex> lock(m_record_enabled_mutex);
    m_record_enabled = on ? true : false;
    if (m_record_enabled)
    {
        m_recorder->startNewRecord();
    }
    else
    {
        m_recorder->stopAndSaveRecord();
        std::cerr << "GuiController::onRecordStatusChanged() record path: " << m_recorder->getRecordPath() << std::endl;
    }
}

} // namespace ui
} // namespace data_recorder
} // namespace apps


