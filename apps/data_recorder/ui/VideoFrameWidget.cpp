#include "VideoFrameWidget.h"
#include "./ui_CameraView.h" // uic 生成的头文件


VideoFrameWidget::VideoFrameWidget(QWidget *parent)
    : QWidget(parent), m_ui(std::make_unique<Ui::VideoFrameWidget>())
{
    m_ui->setupUi(this);
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
