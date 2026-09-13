#ifndef APPS_DATA_RECORDER_UI_VIEW_RECORDERWINDOW_HPP
#define APPS_DATA_RECORDER_UI_VIEW_RECORDERWINDOW_HPP

#include "config/UiConfig.hpp"
#include "core/Frame.hpp"

#include <QSize>
#include <QString>
#include <QWidget>

#include <memory>
#include <string>
#include <vector>

QT_BEGIN_NAMESPACE
namespace Ui
{
class RecorderWindow;
}
QT_END_NAMESPACE

namespace apps::data_recorder::ui
{

/**
 * @brief The application window: toolbar plus the frame grid.
 *
 * It reports what the user asked for and renders what it is given. Which data
 * source is active and whether recording is on is decided by the controller,
 * so the window never has to be queried for application state.
 */
class RecorderWindow : public QWidget
{
    Q_OBJECT

public:
    explicit RecorderWindow(const UiConfig& config, QWidget* parent = nullptr);
    ~RecorderWindow() override;

    void setFrame(const std::string& sourceName, FramePtr frame);
    void clearFrames();

    std::shared_ptr<Frame> grabComposite(const QSize& size, PixelFormat format);

    /// Reflects the recording state the controller actually achieved.
    void setRecordingState(bool recording, const QString& message);

signals:
    void dataSourceSelected(apps::data_recorder::ui::DataSource source);
    void recordToggled(bool enabled);

private:
    std::unique_ptr<Ui::RecorderWindow> m_ui;
};

} // namespace apps::data_recorder::ui

#endif // APPS_DATA_RECORDER_UI_VIEW_RECORDERWINDOW_HPP
