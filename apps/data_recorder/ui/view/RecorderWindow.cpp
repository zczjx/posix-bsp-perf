#include "view/RecorderWindow.hpp"

#include "view/FrameGridView.hpp"

#include "ui_RecorderWindow.h"

#include <QComboBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVariant>

#include <utility>

namespace apps::data_recorder::ui
{

RecorderWindow::RecorderWindow(const UiConfig& config, QWidget* parent)
    : QWidget(parent)
    , m_ui(std::make_unique<Ui::RecorderWindow>())
{
    m_ui->setupUi(this);

    // Each entry carries its enum value, so selection never depends on the
    // displayed label matching a hard-coded string.
    for (const DataSource source: {DataSource::RawCamera, DataSource::ObjectDetection})
    {
        m_ui->dataSourceComboBox->addItem(QString::fromUtf8(toString(source)),
            QVariant::fromValue(static_cast<int>(source)));
    }

    m_ui->frameGrid->configure(config.grid, resolvePlacements(config));

    connect(m_ui->dataSourceComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this](int index) {
            emit dataSourceSelected(static_cast<DataSource>(
                m_ui->dataSourceComboBox->itemData(index).toInt()));
        });

    connect(m_ui->recordButton, &QPushButton::toggled, this, &RecorderWindow::recordToggled);
}

RecorderWindow::~RecorderWindow() = default;

void RecorderWindow::setFrame(const std::string& sourceName, FramePtr frame)
{
    m_ui->frameGrid->setFrame(sourceName, std::move(frame));
}

void RecorderWindow::clearFrames()
{
    m_ui->frameGrid->clearFrames();
}

std::shared_ptr<Frame> RecorderWindow::grabComposite(const QSize& size, PixelFormat format)
{
    return m_ui->frameGrid->grabComposite(size, format);
}

void RecorderWindow::setRecordingState(bool recording, const QString& message)
{
    m_ui->recordButton->setText(recording ? QStringLiteral("Record ON")
                                          : QStringLiteral("Record OFF"));

    // Lets the controller revert the button after a failed start without the
    // change bouncing back as another toggle request.
    if (m_ui->recordButton->isChecked() != recording)
    {
        const QSignalBlocker blocker(m_ui->recordButton);
        m_ui->recordButton->setChecked(recording);
    }

    m_ui->statusLabel->setText(message);
}

} // namespace apps::data_recorder::ui
