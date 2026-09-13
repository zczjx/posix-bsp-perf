#include "model/FrameHub.hpp"

#include "core/Logging.hpp"
#include "model/CameraFrameDecoder.hpp"
#include "model/DetectionFrameDecoder.hpp"

namespace apps::data_recorder::ui
{

namespace
{

std::unique_ptr<FrameDecoder> makeDecoder(StreamKind kind)
{
    switch (kind)
    {
    case StreamKind::Camera:
    case StreamKind::Svs:
        return std::make_unique<CameraFrameDecoder>();
    case StreamKind::ObjectDetector:
        return std::make_unique<DetectionFrameDecoder>();
    }

    return nullptr;
}

} // namespace

FrameHub::FrameHub(const UiConfig& config, QObject* parent)
    : QObject(parent)
{
    for (const auto& streamConfig: config.streams)
    {
        auto decoder = makeDecoder(streamConfig.kind);
        if (!decoder)
        {
            continue;
        }

        auto source = std::make_unique<ShmFrameSource>(streamConfig, std::move(decoder));
        if (!source->open())
        {
            continue;
        }

        const DataSource dataSource = source->dataSource();
        connect(source.get(), &ShmFrameSource::frameReady, this,
            [this, dataSource](const QString& sourceName, FramePtr frame) {
                emit frameReady(sourceName, dataSource, std::move(frame));
            });

        m_sources.push_back(std::move(source));
    }

    qCInfo(uiLog, "Attached to %zu of %zu configured streams", m_sources.size(),
        config.streams.size());
}

FrameHub::~FrameHub()
{
    stop();
}

void FrameHub::start()
{
    for (auto& source: m_sources)
    {
        source->start();
    }
}

void FrameHub::stop()
{
    for (auto& source: m_sources)
    {
        source->stop();
    }
}

} // namespace apps::data_recorder::ui
