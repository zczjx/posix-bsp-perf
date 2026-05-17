#ifndef __VIDEOFRAMEWIDGET_H__
#define __VIDEOFRAMEWIDGET_H__

#include <QWidget>
#include <QComboBox>
#include <QImage>
#include <nlohmann/json.hpp>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

using json = nlohmann::json;

QT_BEGIN_NAMESPACE
namespace Ui {
class VideoFrameWidget;
}
QT_END_NAMESPACE

class FrameWidget;
class QGridLayout;

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
    VideoFrameWidget(const json& gui_ipc, QWidget* parent = nullptr);
    ~VideoFrameWidget();

    void setFrame(const QImage& image);
    void setFrame(const uint8_t* data, int width, int height, std::string format);
    void setFrame(const std::string& sourceName, const uint8_t* data, int width, int height, std::string format);
    void setupVideoGrid(int rows, int columns);
    void setupVideoGrid(const json& gui_ipc);

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
    void clearFrames();

    std::unique_ptr<Ui::VideoFrameWidget> m_ui;
    QGridLayout* m_videoGridLayout{nullptr};
    std::vector<FrameWidget*> m_frameWidgets;
    std::unordered_map<std::string, FrameWidget*> m_frameWidgetMap;
    DataSourceType m_currentDataSource;
    std::mutex m_data_source_mutex;
};

#endif // VIDEOFRAMEWIDGET_H