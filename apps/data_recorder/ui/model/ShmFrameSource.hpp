#ifndef APPS_DATA_RECORDER_UI_MODEL_SHMFRAMESOURCE_HPP
#define APPS_DATA_RECORDER_UI_MODEL_SHMFRAMESOURCE_HPP

#include "config/UiConfig.hpp"
#include "core/Frame.hpp"
#include "model/FrameDecoder.hpp"

#include <QObject>
#include <QString>

#include <atomic>
#include <memory>
#include <string>
#include <thread>

namespace apps::data_recorder::ui
{

/**
 * @brief One shared-memory stream: subscription, worker thread and frame pool.
 *
 * Message-specific knowledge lives in the injected FrameDecoder, so this class
 * is the only consumer loop in the application regardless of how many stream
 * kinds exist.
 *
 * Frames are published as FramePtr, which is copyable and immutable, so the
 * queued connection to the GUI thread hands over ownership instead of a pointer
 * into a buffer this thread is about to refill.
 */
class ShmFrameSource : public QObject
{
    Q_OBJECT

public:
    ShmFrameSource(StreamConfig config, std::unique_ptr<FrameDecoder> decoder,
        QObject* parent = nullptr);
    ~ShmFrameSource() override;

    const std::string& name() const { return m_config.name; }
    DataSource dataSource() const { return dataSourceOf(m_config.kind); }

    /**
     * @brief Attaches to the publisher's shared memory segment.
     * @return false when the publisher is not running yet. This is reported but
     *         not fatal, so one missing node cannot stop the UI from starting.
     */
    bool open();

    void start();
    void stop();

signals:
    void frameReady(const QString& sourceName, apps::data_recorder::ui::FramePtr frame);

private:
    void receiveLoop();

    StreamConfig m_config;
    std::unique_ptr<FrameDecoder> m_decoder;
    std::shared_ptr<midware::zeromq_ipc::SharedMemSubscriber> m_subscriber;
    std::thread m_thread;
    std::atomic<bool> m_stopRequested{false};
};

} // namespace apps::data_recorder::ui

#endif // APPS_DATA_RECORDER_UI_MODEL_SHMFRAMESOURCE_HPP
