#ifndef APPS_DATA_RECORDER_UI_APPOPTIONS_HPP
#define APPS_DATA_RECORDER_UI_APPOPTIONS_HPP

#include <string>

namespace apps::data_recorder::ui
{

/**
 * @brief Everything the application takes from the command line.
 *
 * Parsed once in main and handed to whoever needs it, so no component reaches
 * back into argv and no option is declared in two places with two defaults.
 */
struct AppOptions
{
    std::string nodesIpcFile{"nodes_ipc.json"};
    std::string encoderType;
    std::string g2dType;
    std::string muxerType{"FFmpegMuxer"};
    std::string outputDir;

    /// @return false when the arguments could not be parsed.
    static bool parse(int argc, char* argv[], AppOptions& options);
};

} // namespace apps::data_recorder::ui

#endif // APPS_DATA_RECORDER_UI_APPOPTIONS_HPP
