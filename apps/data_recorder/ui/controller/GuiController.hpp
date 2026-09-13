#ifndef APPS_DATA_RECORDER_UI_CONTROLLER_GUICONTROLLER_HPP
#define APPS_DATA_RECORDER_UI_CONTROLLER_GUICONTROLLER_HPP

#include "config/UiConfig.hpp"
#include "core/Frame.hpp"
#include "model/FrameHub.hpp"
#include "model/Recorder.hpp"
#include "view/RecorderWindow.hpp"

#include <QObject>
#include <QString>
#include <QTimer>

#include <memory>

namespace apps::data_recorder::ui
{

/**
 * @brief Wires the streams, the window and the recorder together.
 *
 * This is the only place that holds application state: which data source is
 * selected and whether recording is on. Both live on the GUI thread and need no
 * locking, because frames reach this object through queued connections.
 *
 * Recording is driven by a timer rather than by frame arrival, so the output
 * frame rate does not depend on which camera happens to deliver first.
 */
class GuiController : public QObject
{
    Q_OBJECT

public:
    GuiController(const UiConfig& config, RecorderWindow& window, FrameHub& hub,
        Recorder& recorder, QObject* parent = nullptr);

private slots:
    void onFrameReady(const QString& sourceName, apps::data_recorder::ui::DataSource source,
        apps::data_recorder::ui::FramePtr frame);
    void onDataSourceSelected(apps::data_recorder::ui::DataSource source);
    void onRecordToggled(bool enabled);
    void onRecordTick();

private:
    const UiConfig& m_config;
    RecorderWindow& m_window;
    FrameHub& m_hub;
    Recorder& m_recorder;

    QTimer m_recordTimer;
    DataSource m_activeSource{DataSource::RawCamera};
};

} // namespace apps::data_recorder::ui

#endif // APPS_DATA_RECORDER_UI_CONTROLLER_GUICONTROLLER_HPP
