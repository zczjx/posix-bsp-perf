#ifndef __VIDEOFRAMEWIDGET_H__
#define __VIDEOFRAMEWIDGET_H__

#include <QWidget>
#include <memory>


QT_BEGIN_NAMESPACE
namespace Ui {
class VideoFrameWidget;
}
QT_END_NAMESPACE

class VideoFrameWidget : public QWidget
{
    Q_OBJECT
public:
    VideoFrameWidget(QWidget* parent = nullptr);
    ~VideoFrameWidget();

    void setFrame(const QImage& image);
    void setFrame(const uint8_t* data, int width, int height);


private:
    std::unique_ptr<Ui::VideoFrameWidget> m_ui;
};


#endif // VIDEOFRAMEWIDGET_H