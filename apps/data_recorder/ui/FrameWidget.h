#ifndef __FRAMEWIDGET_H__
#define __FRAMEWIDGET_H__
#include <QWidget>
#include <QImage>
#include <QMutex>
#include <QPainter>

class FrameWidget : public QWidget
{
    Q_OBJECT
public:
    explicit FrameWidget(QWidget* parent = nullptr)
        : QWidget(parent)
    {}

    void setFrame(const QImage& img)
    {
        QMutexLocker locker(&m_mutex);
        m_img = img.copy();
        update();
    }

    void setFrame(const uint8_t* data, int width, int height)
    {
        QMutexLocker locker(&m_mutex);
        m_img = QImage(data, width, height, QImage::Format_RGB888).copy();
        update();
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);
        QPainter painter(this);
        QMutexLocker locker(&m_mutex);

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

private:
    QImage m_img;
    QMutex m_mutex;
};

#endif // FRAMEWIDGET_H