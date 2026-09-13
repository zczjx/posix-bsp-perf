#ifndef APPS_DATA_RECORDER_UI_CONFIG_UICONFIG_HPP
#define APPS_DATA_RECORDER_UI_CONFIG_UICONFIG_HPP

#include <nlohmann/json.hpp>

#include <QMetaType>

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace apps::data_recorder::ui
{

/// Which of the mutually exclusive views the user can select in the toolbar.
enum class DataSource
{
    RawCamera,
    ObjectDetection,
};

const char* toString(DataSource source);

/// The producer node behind a stream. Several kinds map onto one DataSource.
enum class StreamKind
{
    Camera,
    Svs,
    ObjectDetector,
};

DataSource dataSourceOf(StreamKind kind);

struct ShmEndpoint
{
    std::string topic;
    std::string shmemName;
    // Not named "slots": Qt defines that as a macro.
    std::size_t slotCount{0};
    std::size_t singleBufferSize{0};
};

/// A grid cell. A negative coordinate means "place me automatically".
struct GridPosition
{
    int row{-1};
    int column{-1};

    bool isExplicit() const { return row >= 0 && column >= 0; }
};

struct StreamConfig
{
    std::string name;
    StreamKind kind{StreamKind::Camera};
    ShmEndpoint publisher;
    GridPosition displayPosition;
};

struct GridSize
{
    int rows{1};
    int columns{1};

    int cellCount() const { return rows * columns; }
};

/// A stream resolved onto a concrete, in-bounds grid cell.
struct StreamPlacement
{
    std::string name;
    int row{0};
    int column{0};
};

class ConfigError : public std::runtime_error
{
public:
    explicit ConfigError(const std::string& what)
        : std::runtime_error(what)
    {
    }
};

/**
 * @brief The whole UI configuration, parsed once at start-up.
 *
 * Everything downstream consumes these structs rather than the raw json, so the
 * file layout is described in exactly one place and the view and model can no
 * longer disagree about which streams are enabled or where they belong.
 */
struct UiConfig
{
    GridSize grid;
    std::vector<StreamConfig> streams;
    int recordWidth{1280};
    int recordHeight{720};
    int recordFps{30};

    int recordIntervalMs() const { return 1000 / (recordFps > 0 ? recordFps : 1); }

    /// @throws ConfigError when the document is not a usable node-ipc object.
    /// Individual malformed stream entries are skipped with a warning instead of
    /// failing the whole launch.
    static UiConfig fromJson(const nlohmann::json& root);

    std::vector<const StreamConfig*> streamsOf(DataSource source) const;
};

/**
 * @brief Assigns every stream a concrete cell.
 *
 * Streams with an in-bounds display_position keep it; the rest fill the cells
 * nobody claimed, in declaration order. Streams that cannot be placed because
 * the grid is full are dropped from the result.
 */
std::vector<StreamPlacement> resolvePlacements(const UiConfig& config);

} // namespace apps::data_recorder::ui

Q_DECLARE_METATYPE(apps::data_recorder::ui::DataSource)

#endif // APPS_DATA_RECORDER_UI_CONFIG_UICONFIG_HPP
