#ifndef __VIDEOFRAMEWIDGET_H__
#define __VIDEOFRAMEWIDGET_H__

#include <QWidget>

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


private:
    Ui::VideoFrameWidget *ui;
};

#endif // VIDEOFRAMEWIDGET_H