#include "FrameWidget.h"
#include <QPainter>
#include <iostream>

void FrameWidget::setFrame(const QImage& img)
{
    QMutexLocker locker(&m_mutex);

    if (m_img.size() != img.size() || m_img.format() != img.format()) {
        m_img = QImage(img.size(), img.format());
    }

    memcpy(m_img.bits(), img.bits(), m_img.sizeInBytes());
    update();
}

void FrameWidget::setFrame(const uint8_t* data, int width, int height)
{
    QMutexLocker locker(&m_mutex);

    if (!m_img_initialized || m_img.width() != width || m_img.height() != height)
    {
        m_img = QImage(width, height, QImage::Format_RGB888);
        m_img_initialized = true;
    }

    // 拷贝数据到 QImage 内部 buffer
    memcpy(m_img.bits(), data, width * height * 3);
    update();
}

void FrameWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    QMutexLocker locker(&m_mutex);

    if (!m_timer_started)
    {
        m_timer.start();
        m_timer_started = true;
    }

    m_frame_count++;
    if (m_timer.elapsed() >= 1000)
    {
        std::cout << "FrameWidget::paintEvent() FPS: " << m_frame_count << std::endl;
        m_frame_count = 0;
        m_timer.restart();
    }

    if (!m_img.isNull())
    {
        QSize widgetSize = size();
        QSize imgSize = m_img.size();
        imgSize.scale(widgetSize, Qt::KeepAspectRatio);

        QPoint topLeft((widgetSize.width() - imgSize.width()) / 2,
                        (widgetSize.height() - imgSize.height()) / 2);

        painter.fillRect(rect(), Qt::black);
        painter.drawImage(QRect(topLeft, imgSize), m_img);
    }
    else
    {
        painter.fillRect(rect(), Qt::black);
    }
}