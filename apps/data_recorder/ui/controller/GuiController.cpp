#include "GuiController.hpp"
#include <cstring>
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
    m_video_frame_widget = std::make_unique<VideoFrameWidget>(gui_ipc);
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

int GuiController::updateFrameRecord()
{
    if (!m_record_enabled)
    {
        return 0;
    }

    if (m_record_frame_timer.isValid() && m_record_frame_timer.elapsed() < m_record_interval_ms)
    {
        return 0;
    }

    QImage compositeFrame = m_video_frame_widget->grabCompositeFrame();
    if (compositeFrame.isNull())
    {
        return -1;
    }

    const int width = compositeFrame.width();
    const int height = compositeFrame.height();
    const int packedLineSize = width * 4;
    uint8_t* frameData = compositeFrame.bits();
    std::vector<uint8_t> packedFrame;

    if (compositeFrame.bytesPerLine() != packedLineSize)
    {
        packedFrame.resize(static_cast<size_t>(packedLineSize) * height);
        for (int row = 0; row < height; ++row)
        {
            std::memcpy(packedFrame.data() + static_cast<size_t>(row) * packedLineSize,
                compositeFrame.constScanLine(row), packedLineSize);
        }
        frameData = packedFrame.data();
    }

    int ret = m_recorder->writeRecordFrame(frameData, width, height, "RGBA8888");
    m_record_frame_timer.restart();
    return ret;
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

void GuiController::onRawCameraFrameUpdated(const QString& sensorName, uint8_t* data, int width, int height, const QString& format)
{
    if (m_video_frame_widget->getCurrentDataSource() == VideoFrameWidget::RawCamera)
    {
        std::cout << "GuiController::onRawCameraFrameUpdated() sensor: " << sensorName.toStdString() << " width: " << width << " height: " << height << std::endl;
        m_video_frame_widget->setFrame(sensorName.toStdString(), data, width, height, format.toStdString());
        updateFrameRecord();
    }
}

void GuiController::onObjectsDetectionFrameUpdated(const QString& detectorName, uint8_t* data, int width, int height, const QString& format)
{
    if (m_video_frame_widget->getCurrentDataSource() == VideoFrameWidget::ObjectsDetection)
    {
        std::cout << "GuiController::onObjectsDetectionFrameUpdated() detector: " << detectorName.toStdString() << " width: " << width << " height: " << height << std::endl;
        m_video_frame_widget->setFrame(detectorName.toStdString(), data, width, height, format.toStdString());
        updateFrameRecord();
    }
}

void GuiController::onRecordStatusChanged(bool on)
{
    std::lock_guard<std::mutex> lock(m_record_enabled_mutex);
    m_record_enabled = on ? true : false;
    if (m_record_enabled)
    {
        m_recorder->startNewRecord();
        m_record_frame_timer.invalidate();
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


