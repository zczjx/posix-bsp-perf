#include "VideoFrameWidget.h"
#include "FrameWidget.h"
#include "ui_CameraView.h" // uic 生成的头文件
#include <QDebug>
#include <QGridLayout>
#include <QPixmap>
#include <QSize>
#include <algorithm>

VideoFrameWidget::VideoFrameWidget(QWidget *parent)
    : QWidget(parent), m_ui(std::make_unique<Ui::VideoFrameWidget>()), m_currentDataSource(RawCamera)
{
    m_ui->setupUi(this);
    m_videoGridLayout = m_ui->videoGridLayout;

    // 初始化数据源下拉菜单
    initializeDataSourceComboBox();
    setupVideoGrid(1, 1);

    // 设置信号槽连接
    setupConnections();
}

VideoFrameWidget::VideoFrameWidget(const json& gui_ipc, QWidget *parent)
    : QWidget(parent), m_ui(std::make_unique<Ui::VideoFrameWidget>()), m_currentDataSource(RawCamera)
{
    m_ui->setupUi(this);
    m_videoGridLayout = m_ui->videoGridLayout;

    // 初始化数据源下拉菜单
    initializeDataSourceComboBox();
    setupVideoGrid(gui_ipc);

    // 设置信号槽连接
    setupConnections();
}

VideoFrameWidget::~VideoFrameWidget()
{
    m_ui.reset();
}

void VideoFrameWidget::setFrame(const QImage& image)
{
    if (!m_frameWidgets.empty())
    {
        m_frameWidgets.front()->setFrame(image);
    }
}

void VideoFrameWidget::setFrame(const uint8_t* data, int width, int height, std::string format)
{
    if (!m_frameWidgets.empty())
    {
        m_frameWidgets.front()->setFrame(data, width, height, format);
    }
}

void VideoFrameWidget::setFrame(const std::string& sourceName, const uint8_t* data, int width, int height, std::string format)
{
    auto frameWidgetIt = m_frameWidgetMap.find(sourceName);
    if (frameWidgetIt != m_frameWidgetMap.end())
    {
        frameWidgetIt->second->setFrame(data, width, height, format);
        return;
    }

    setFrame(data, width, height, format);
}

void VideoFrameWidget::setupVideoGrid(int rows, int columns)
{
    if (rows <= 0 || columns <= 0)
    {
        qWarning() << "Invalid video grid size:" << rows << "x" << columns;
        return;
    }

    while (QLayoutItem* item = m_videoGridLayout->takeAt(0))
    {
        if (QWidget* widget = item->widget())
        {
            widget->deleteLater();
        }
        delete item;
    }

    m_frameWidgets.clear();
    m_frameWidgetMap.clear();
    m_frameWidgets.reserve(rows * columns);

    for (int row = 0; row < rows; ++row)
    {
        m_videoGridLayout->setRowStretch(row, 1);
        for (int column = 0; column < columns; ++column)
        {
            auto* frameWidget = new FrameWidget(m_ui->videoGridContainer);
            frameWidget->setMinimumSize(160, 120);
            frameWidget->setStyleSheet("background-color: black;");
            m_videoGridLayout->addWidget(frameWidget, row, column);
            m_videoGridLayout->setColumnStretch(column, 1);
            m_frameWidgets.push_back(frameWidget);
        }
    }
}

void VideoFrameWidget::setupVideoGrid(const json& gui_ipc)
{
    std::vector<std::string> cameraNames;
    if (gui_ipc.contains("camera") && gui_ipc["camera"].is_array())
    {
        for (const auto& camera: gui_ipc["camera"])
        {
            if (camera.value("status", std::string("enabled")) != "enabled")
            {
                continue;
            }

            cameraNames.push_back(camera.value("name", std::string()));
        }
    }

    int rows = 1;
    int columns = 1;
    bool hasConfiguredGrid = false;

    if (gui_ipc.contains("ui") && gui_ipc["ui"].contains("grid_size"))
    {
        const auto& gridSize = gui_ipc["ui"]["grid_size"];
        if (gridSize.is_array() && gridSize.size() >= 2)
        {
            rows = std::max(1, gridSize[0].get<int>());
            columns = std::max(1, gridSize[1].get<int>());
            hasConfiguredGrid = true;
        }
    }
    else if (gui_ipc.contains("gui") && gui_ipc["gui"].is_array() && !gui_ipc["gui"].empty() && gui_ipc["gui"][0].contains("grid_size"))
    {
        const auto& gridSize = gui_ipc["gui"][0]["grid_size"];
        if (gridSize.is_array() && gridSize.size() >= 2)
        {
            rows = std::max(1, gridSize[0].get<int>());
            columns = std::max(1, gridSize[1].get<int>());
            hasConfiguredGrid = true;
        }
    }

    if (!hasConfiguredGrid)
    {
        const int cameraCount = std::max(1, static_cast<int>(cameraNames.size()));
        while (columns * columns < cameraCount)
        {
            ++columns;
        }
        rows = (cameraCount + columns - 1) / columns;
    }

    setupVideoGrid(rows, columns);

    if (gui_ipc.contains("camera") && gui_ipc["camera"].is_array())
    {
        int nextIndex = 0;
        for (const auto& camera: gui_ipc["camera"])
        {
            if (camera.value("status", std::string("enabled")) != "enabled")
            {
                continue;
            }

            const std::string cameraName = camera.value("name", std::string());
            if (cameraName.empty())
            {
                continue;
            }

            int row = nextIndex / columns;
            int column = nextIndex % columns;
            if (camera.contains("display_position"))
            {
                const auto& displayPosition = camera["display_position"];
                if (displayPosition.is_array() && displayPosition.size() >= 2)
                {
                    row = displayPosition[0].get<int>();
                    column = displayPosition[1].get<int>();
                }
            }

            if (row >= 0 && row < rows && column >= 0 && column < columns)
            {
                m_frameWidgetMap[cameraName] = m_frameWidgets[row * columns + column];
            }

            ++nextIndex;
        }
    }

    if (gui_ipc.contains("object_detector") && gui_ipc["object_detector"].is_array())
    {
        for (const auto& detector: gui_ipc["object_detector"])
        {
            if (detector.value("status", std::string("enabled")) != "enabled")
            {
                continue;
            }

            const std::string detectorName = detector.value("name", std::string());
            if (detectorName.empty() || !detector.contains("display_position"))
            {
                continue;
            }

            const auto& displayPosition = detector["display_position"];
            if (!displayPosition.is_array() || displayPosition.size() < 2)
            {
                continue;
            }

            const int row = displayPosition[0].get<int>();
            const int column = displayPosition[1].get<int>();
            if (row >= 0 && row < rows && column >= 0 && column < columns)
            {
                m_frameWidgetMap[detectorName] = m_frameWidgets[row * columns + column];
            }
        }
    }
}

QImage VideoFrameWidget::grabCompositeFrame()
{
    if (!m_ui || !m_ui->videoGridContainer)
    {
        return {};
    }

    QImage image = m_ui->videoGridContainer->grab().toImage();
    if (image.size() != QSize(1280, 720))
    {
        image = image.scaled(1280, 720, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    image = image.convertToFormat(QImage::Format_RGB888);
    const int evenWidth = image.width() & ~1;
    const int evenHeight = image.height() & ~1;
    if (evenWidth <= 0 || evenHeight <= 0)
    {
        return {};
    }

    if (evenWidth != image.width() || evenHeight != image.height())
    {
        image = image.copy(0, 0, evenWidth, evenHeight);
    }

    return image;
}

void VideoFrameWidget::clearFrames()
{
    for (auto* frameWidget: m_frameWidgets)
    {
        if (frameWidget)
        {
            frameWidget->clearFrame();
        }
    }
}

void VideoFrameWidget::onDataSourceChanged(const QString& text)
{
    DataSourceType newSource = ObjectsDetection; // 默认值

    if (text == "Raw Camera")
    {
        newSource = RawCamera;
    }
    else if (text == "Objects Detection")
    {
        newSource = ObjectsDetection;
    }

    if (m_currentDataSource != newSource)
    {
        std::lock_guard<std::mutex> lock(m_data_source_mutex);
        m_currentDataSource = newSource;
        clearFrames();

        // 输出调试信息
        qDebug() << "数据源已切换到:" << text << "(" << static_cast<int>(newSource) << ")";
    }
}

void VideoFrameWidget::setupConnections()
{
    // 连接下拉菜单的信号
    connect(m_ui->dataSourceComboBox, QOverload<const QString &>::of(&QComboBox::currentTextChanged),
            this, &VideoFrameWidget::onDataSourceChanged);

    connect(m_ui->toggleBtn, &QPushButton::toggled, this, [this](bool on){
        m_ui->toggleBtn->setText(on ? "Record ON" : "Record OFF");
        emit recordStatusChanged(on);
    });
}

void VideoFrameWidget::initializeDataSourceComboBox()
{
    // 确保下拉菜单包含正确的选项
    if (m_ui->dataSourceComboBox->count() == 0) {
        m_ui->dataSourceComboBox->addItem("Raw Camera");
        m_ui->dataSourceComboBox->addItem("Objects Detection");
    }

    // 设置默认选择
    std::lock_guard<std::mutex> lock(m_data_source_mutex);
    m_ui->dataSourceComboBox->setCurrentIndex(static_cast<int>(m_currentDataSource));
}
