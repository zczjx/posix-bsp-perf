#include "UiConfig.hpp"

#include "core/Logging.hpp"

#include <algorithm>
#include <vector>

namespace apps::data_recorder::ui
{

namespace
{

constexpr int kDefaultRecordWidth = 1280;
constexpr int kDefaultRecordHeight = 720;
constexpr int kDefaultRecordFps = 30;

bool isEnabled(const nlohmann::json& node)
{
    return node.value("status", std::string("enabled")) == "enabled";
}

/**
 * @brief Locates the object holding window-level settings.
 *
 * Two layouts exist in the shipped rig files: a top-level "ui" object and a
 * "gui" array whose first enabled entry carries the same keys. Both are
 * accepted here so the rest of the code never has to know about either.
 */
const nlohmann::json* findUiSettings(const nlohmann::json& root)
{
    if (auto it = root.find("ui"); it != root.end() && it->is_object())
    {
        return &*it;
    }

    if (auto it = root.find("gui"); it != root.end() && it->is_array())
    {
        for (const auto& entry: *it)
        {
            if (entry.is_object() && isEnabled(entry))
            {
                return &entry;
            }
        }
    }

    return nullptr;
}

/// Reads a two-element integer array such as "grid_size" or "record_size".
bool readIntPair(const nlohmann::json& parent, const char* key, int& first, int& second)
{
    auto it = parent.find(key);
    if (it == parent.end() || !it->is_array() || it->size() < 2)
    {
        return false;
    }

    if (!(*it)[0].is_number_integer() || !(*it)[1].is_number_integer())
    {
        return false;
    }

    first = (*it)[0].get<int>();
    second = (*it)[1].get<int>();
    return true;
}

bool readEndpoint(const nlohmann::json& node, ShmEndpoint& endpoint)
{
    auto it = node.find("publisher");
    if (it == node.end() || !it->is_object())
    {
        return false;
    }

    const nlohmann::json& publisher = *it;
    if (!publisher.contains("topic") || !publisher.contains("shmem"))
    {
        return false;
    }

    endpoint.topic = publisher.value("topic", std::string());
    endpoint.shmemName = publisher.value("shmem", std::string());
    endpoint.slotCount = publisher.value("shmem_slots", std::size_t{0});
    endpoint.singleBufferSize = publisher.value("shmem_single_buffer_size", std::size_t{0});

    return !endpoint.topic.empty() && !endpoint.shmemName.empty()
        && endpoint.slotCount > 0 && endpoint.singleBufferSize > 0;
}

/// Appends every enabled, well-formed entry of one json section.
void readStreamSection(const nlohmann::json& root, const char* section, StreamKind kind,
    std::vector<StreamConfig>& streams)
{
    auto it = root.find(section);
    if (it == root.end() || !it->is_array())
    {
        return;
    }

    for (const auto& node: *it)
    {
        if (!node.is_object() || !isEnabled(node))
        {
            continue;
        }

        StreamConfig stream;
        stream.name = node.value("name", std::string());
        stream.kind = kind;

        if (stream.name.empty())
        {
            qCWarning(uiLog, "Ignoring a '%s' entry without a name", section);
            continue;
        }

        if (!readEndpoint(node, stream.publisher))
        {
            qCWarning(uiLog, "Ignoring stream '%s': incomplete publisher endpoint",
                stream.name.c_str());
            continue;
        }

        readIntPair(node, "display_position", stream.displayPosition.row,
            stream.displayPosition.column);

        streams.push_back(std::move(stream));
    }
}

/// Smallest square-ish grid that fits the given number of cells.
GridSize autoGrid(std::size_t streamCount)
{
    const int count = std::max(1, static_cast<int>(streamCount));

    int columns = 1;
    while (columns * columns < count)
    {
        ++columns;
    }

    return GridSize{(count + columns - 1) / columns, columns};
}

/// Grows the grid until it covers every explicitly requested cell.
GridSize expandForExplicitPositions(GridSize grid, const std::vector<StreamConfig>& streams)
{
    for (const auto& stream: streams)
    {
        if (!stream.displayPosition.isExplicit())
        {
            continue;
        }

        grid.rows = std::max(grid.rows, stream.displayPosition.row + 1);
        grid.columns = std::max(grid.columns, stream.displayPosition.column + 1);
    }

    return grid;
}

} // namespace

const char* toString(DataSource source)
{
    switch (source)
    {
    case DataSource::RawCamera:
        return "Raw Camera";
    case DataSource::ObjectDetection:
        return "Objects Detection";
    }

    return "Unknown";
}

DataSource dataSourceOf(StreamKind kind)
{
    switch (kind)
    {
    case StreamKind::Camera:
    case StreamKind::Svs:
        return DataSource::RawCamera;
    case StreamKind::ObjectDetector:
        return DataSource::ObjectDetection;
    }

    return DataSource::RawCamera;
}

UiConfig UiConfig::fromJson(const nlohmann::json& root)
{
    if (!root.is_object())
    {
        throw ConfigError("node ipc configuration must be a json object");
    }

    UiConfig config;

    readStreamSection(root, "camera", StreamKind::Camera, config.streams);
    readStreamSection(root, "svs", StreamKind::Svs, config.streams);
    readStreamSection(root, "object_detector", StreamKind::ObjectDetector, config.streams);

    if (config.streams.empty())
    {
        throw ConfigError("node ipc configuration declares no enabled camera, svs or "
                          "object_detector stream");
    }

    bool gridConfigured = false;

    if (const nlohmann::json* settings = findUiSettings(root); settings != nullptr)
    {
        gridConfigured = readIntPair(*settings, "grid_size", config.grid.rows, config.grid.columns);
        readIntPair(*settings, "record_size", config.recordWidth, config.recordHeight);

        if (auto it = settings->find("record_fps"); it != settings->end() && it->is_number())
        {
            config.recordFps = it->get<int>();
        }
    }

    if (!gridConfigured)
    {
        config.grid = autoGrid(config.streams.size());
    }

    config.grid.rows = std::max(1, config.grid.rows);
    config.grid.columns = std::max(1, config.grid.columns);
    config.grid = expandForExplicitPositions(config.grid, config.streams);

    config.recordWidth = config.recordWidth > 0 ? config.recordWidth : kDefaultRecordWidth;
    config.recordHeight = config.recordHeight > 0 ? config.recordHeight : kDefaultRecordHeight;
    config.recordFps = config.recordFps > 0 ? config.recordFps : kDefaultRecordFps;

    return config;
}

std::vector<const StreamConfig*> UiConfig::streamsOf(DataSource source) const
{
    std::vector<const StreamConfig*> selected;
    for (const auto& stream: streams)
    {
        if (dataSourceOf(stream.kind) == source)
        {
            selected.push_back(&stream);
        }
    }

    return selected;
}

std::vector<StreamPlacement> resolvePlacements(const UiConfig& config)
{
    const int rows = config.grid.rows;
    const int columns = config.grid.columns;

    std::vector<bool> taken(static_cast<std::size_t>(rows) * columns, false);
    std::vector<StreamPlacement> placements;
    placements.reserve(config.streams.size());

    // Streams still needing a cell after the explicit pass, as indices into
    // config.streams so the automatic pass keeps declaration order.
    std::vector<std::size_t> pending;

    for (std::size_t i = 0; i < config.streams.size(); ++i)
    {
        const StreamConfig& stream = config.streams[i];
        const GridPosition& position = stream.displayPosition;

        if (!position.isExplicit() || position.row >= rows || position.column >= columns)
        {
            pending.push_back(i);
            continue;
        }

        const std::size_t cell = static_cast<std::size_t>(position.row) * columns + position.column;
        if (taken[cell])
        {
            qCWarning(uiLog, "Stream '%s' requests cell (%d,%d) which is already taken; "
                             "placing it automatically",
                stream.name.c_str(), position.row, position.column);
            pending.push_back(i);
            continue;
        }

        taken[cell] = true;
        placements.push_back({stream.name, position.row, position.column});
    }

    std::size_t nextFree = 0;
    for (const std::size_t index: pending)
    {
        while (nextFree < taken.size() && taken[nextFree])
        {
            ++nextFree;
        }

        if (nextFree >= taken.size())
        {
            qCWarning(uiLog, "No free grid cell left for stream '%s'",
                config.streams[index].name.c_str());
            continue;
        }

        taken[nextFree] = true;
        placements.push_back({config.streams[index].name,
            static_cast<int>(nextFree) / columns, static_cast<int>(nextFree) % columns});
    }

    return placements;
}

} // namespace apps::data_recorder::ui
