#ifndef __FRAMEWIDGET_H__
#define __FRAMEWIDGET_H__
#include <QWidget>
#include <QImage>
#include <QMutex>
#include <QElapsedTimer>

class FrameWidget : public QWidget
{
    Q_OBJECT
public:
    explicit FrameWidget(QWidget* parent = nullptr)
        : QWidget(parent)
    {}

    void setFrame(const QImage& img);

    void setFrame(const uint8_t* data, int width, int height, std::string format = "RGB888");

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QImage m_img;
    QMutex m_mutex;
    bool m_img_initialized{false};

    int m_frame_count{0};
    QElapsedTimer m_timer;
    bool m_timer_started{false};
};

#endif // FRAMEWIDGET_H