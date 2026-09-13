#include "controller/GuiController.hpp"

#include "core/Logging.hpp"

#include <QSize>

#include <utility>

namespace apps::data_recorder::ui
{

namespace
{

#ifdef BUILD_PLATFORM_JETSON
// The Jetson VIC cannot consume 24-bit RGB, so recording goes through RGBA8888.
constexpr PixelFormat kRecordFormat = PixelFormat::Rgba8888;
#else
constexpr PixelFormat kRecordFormat = PixelFormat::Rgb888;
#endif

} // namespace

GuiController::GuiController(const UiConfig& config, RecorderWindow& window, FrameHub& hub,
    Recorder& recorder, QObject* parent)
    : QObject(parent)
    , m_config(config)
    , m_window(window)
    , m_hub(hub)
    , m_recorder(recorder)
{
    connect(&m_hub, &FrameHub::frameReady, this, &GuiController::onFrameReady);
    connect(&m_window, &RecorderWindow::dataSourceSelected, this,
        &GuiController::onDataSourceSelected);
    connect(&m_window, &RecorderWindow::recordToggled, this, &GuiController::onRecordToggled);

    m_recordTimer.setTimerType(Qt::PreciseTimer);
    m_recordTimer.setInterval(m_config.recordIntervalMs());
    connect(&m_recordTimer, &QTimer::timeout, this, &GuiController::onRecordTick);
}

void GuiController::onFrameReady(const QString& sourceName, DataSource source, FramePtr frame)
{
    if (source != m_activeSource)
    {
        return;
    }

    m_window.setFrame(sourceName.toStdString(), std::move(frame));
}

void GuiController::onDataSourceSelected(DataSource source)
{
    if (source == m_activeSource)
    {
        return;
    }

    m_activeSource = source;
    m_window.clearFrames();

    qCInfo(uiLog, "Data source switched to %s", toString(source));
}

void GuiController::onRecordToggled(bool enabled)
{
    if (!enabled)
    {
        m_recordTimer.stop();
        m_recorder.stop();
        m_window.setRecordingState(false, QString::fromStdString(m_recorder.currentPath()));
        return;
    }

    if (!m_recorder.start())
    {
        m_window.setRecordingState(false, QStringLiteral("Recording failed to start"));
        return;
    }

    m_recordTimer.start();
    m_window.setRecordingState(true, QString::fromStdString(m_recorder.currentPath()));
}

void GuiController::onRecordTick()
{
    auto composite = m_window.grabComposite(QSize(m_config.recordWidth, m_config.recordHeight),
        kRecordFormat);
    if (!composite)
    {
        qCWarning(uiLog, "Skipping record tick: nothing to composite");
        return;
    }

    m_recorder.writeFrame(*composite);
}

} // namespace apps::data_recorder::ui
