#include "model/ShmFrameSource.hpp"

#include "core/Logging.hpp"

#include <exception>
#include <utility>
#include <vector>

namespace apps::data_recorder::ui
{

namespace
{

// Bounds how long stop() waits: the worker can only notice the stop request
// between receives, so the subscriber must not block indefinitely.
constexpr int kReceiveTimeoutMs = 200;

} // namespace

ShmFrameSource::ShmFrameSource(StreamConfig config, std::unique_ptr<FrameDecoder> decoder,
    QObject* parent)
    : QObject(parent)
    , m_config(std::move(config))
    , m_decoder(std::move(decoder))
{
}

ShmFrameSource::~ShmFrameSource()
{
    stop();
}

bool ShmFrameSource::open()
{
    try
    {
        m_subscriber = std::make_shared<midware::zeromq_ipc::SharedMemSubscriber>(
            m_config.publisher.topic, m_config.publisher.shmemName,
            m_config.publisher.slotCount, m_config.publisher.singleBufferSize);
    }
    catch (const std::exception& error)
    {
        qCWarning(uiLog, "Stream '%s' unavailable (%s): %s", m_config.name.c_str(),
            m_config.publisher.shmemName.c_str(), error.what());
        return false;
    }

    m_subscriber->setReceiveTimeout(kReceiveTimeoutMs);
    return true;
}

void ShmFrameSource::start()
{
    if (!m_subscriber || m_thread.joinable())
    {
        return;
    }

    m_stopRequested.store(false);
    m_thread = std::thread([this]() { receiveLoop(); });
}

void ShmFrameSource::stop()
{
    m_stopRequested.store(true);
    if (m_thread.joinable())
    {
        m_thread.join();
    }
}

void ShmFrameSource::receiveLoop()
{
    const QString sourceName = QString::fromStdString(m_config.name);
    std::vector<uint8_t> metadata(m_decoder->metadataSize());
    FramePool pool;

    while (!m_stopRequested.load())
    {
        const std::size_t received = m_subscriber->receiveMsg(metadata.data(), metadata.size());
        if (received == 0)
        {
            // Receive timeout, which is how the loop gets a chance to exit.
            continue;
        }

        std::shared_ptr<Frame> frame;
        try
        {
            frame = m_decoder->decode(metadata.data(), received, *m_subscriber, pool);
        }
        catch (const std::exception& error)
        {
            qCWarning(uiLog, "Stream '%s': cannot decode message: %s",
                m_config.name.c_str(), error.what());
            continue;
        }

        if (!frame || frame->empty())
        {
            continue;
        }

        qCDebug(uiFrameLog, "Stream '%s': %dx%d %s", m_config.name.c_str(), frame->width(),
            frame->height(), toString(frame->format()));

        emit frameReady(sourceName, FramePtr(std::move(frame)));
    }
}

} // namespace apps::data_recorder::ui
