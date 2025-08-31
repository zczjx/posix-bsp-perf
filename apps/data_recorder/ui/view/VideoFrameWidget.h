#ifndef __VIDEOFRAMEWIDGET_H__
#define __VIDEOFRAMEWIDGET_H__

#include <QWidget>
#include <QComboBox>
#include <memory>
#include <mutex>

QT_BEGIN_NAMESPACE
namespace Ui {
class VideoFrameWidget;
}
QT_END_NAMESPACE

class VideoFrameWidget : public QWidget
{
    Q_OBJECT
public:
    // 数据源类型枚举
    enum DataSourceType {
        RawCamera = 0,
        ObjectsDetection = 1
    };

    VideoFrameWidget(QWidget* parent = nullptr);
    ~VideoFrameWidget();

    void setFrame(const QImage& image);
    void setFrame(const uint8_t* data, int width, int height);

    DataSourceType getCurrentDataSource()
    {
        std::lock_guard<std::mutex> lock(m_data_source_mutex);
        return m_currentDataSource;
    }


private slots:
    // 处理数据源选择变化
    void onDataSourceChanged(const QString& text);

signals:
    void recordStatusChanged(bool on);

private:
    void setupConnections();
    void initializeDataSourceComboBox();

    std::unique_ptr<Ui::VideoFrameWidget> m_ui;
    DataSourceType m_currentDataSource;
    std::mutex m_data_source_mutex;
};

#endif // VIDEOFRAMEWIDGET_H