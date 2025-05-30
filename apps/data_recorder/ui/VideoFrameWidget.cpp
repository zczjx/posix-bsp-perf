#include "VideoFrameWidget.h"
#include "./ui_CameraView.h" // uic 生成的头文件

VideoFrameWidget::VideoFrameWidget(QWidget *parent)
    : QWidget(parent), ui(new Ui::VideoFrameWidget)
{
    ui->setupUi(this);
}

VideoFrameWidget::~VideoFrameWidget()
{
    delete ui;
}

void VideoFrameWidget::setFrame(const QImage& image)
{
    ui->frameWidget->setFrame(image);
}

void VideoFrameWidget::setFrame(const uint8_t* data, int width, int height)
{
    ui->frameWidget->setFrame(data, width, height);
}
