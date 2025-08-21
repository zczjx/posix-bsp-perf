#include "VideoFrameWidget.h"
#include "ui_CameraView.h" // uic 生成的头文件
#include <QDebug>

VideoFrameWidget::VideoFrameWidget(QWidget *parent)
    : QWidget(parent), m_ui(std::make_unique<Ui::VideoFrameWidget>()), m_currentDataSource(ObjectsDetection)
{
    m_ui->setupUi(this);

    // 初始化数据源下拉菜单
    initializeDataSourceComboBox();

    // 设置信号槽连接
    setupConnections();
}

VideoFrameWidget::~VideoFrameWidget()
{
    m_ui.reset();
}

void VideoFrameWidget::setFrame(const QImage& image)
{
    m_ui->frameWidget->setFrame(image);
}

void VideoFrameWidget::setFrame(const uint8_t* data, int width, int height)
{
    m_ui->frameWidget->setFrame(data, width, height);
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
