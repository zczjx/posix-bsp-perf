#ifndef APPS_DATA_RECORDER_UI_MODEL_FRAMEHUB_HPP
#define APPS_DATA_RECORDER_UI_MODEL_FRAMEHUB_HPP

#include "config/UiConfig.hpp"
#include "core/Frame.hpp"
#include "model/ShmFrameSource.hpp"

#include <QObject>
#include <QString>

#include <memory>
#include <vector>

namespace apps::data_recorder::ui
{

/**
 * @brief Owns every configured stream and merges them into one signal.
 *
 * The controller connects here once instead of knowing which stream kinds
 * exist, so supporting a new producer is a configuration and decoder change
 * rather than new wiring.
 */
class FrameHub : public QObject
{
    Q_OBJECT

public:
    explicit FrameHub(const UiConfig& config, QObject* parent = nullptr);
    ~FrameHub() override;

    /// Number of streams that successfully attached to their publisher.
    std::size_t liveSourceCount() const { return m_sources.size(); }

    void start();
    void stop();

signals:
    void frameReady(const QString& sourceName, apps::data_recorder::ui::DataSource source,
        apps::data_recorder::ui::FramePtr frame);

private:
    std::vector<std::unique_ptr<ShmFrameSource>> m_sources;
};

} // namespace apps::data_recorder::ui

#endif // APPS_DATA_RECORDER_UI_MODEL_FRAMEHUB_HPP
