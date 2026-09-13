#include "AppOptions.hpp"
#include "config/UiConfig.hpp"
#include "controller/GuiController.hpp"
#include "core/Frame.hpp"
#include "core/Logging.hpp"
#include "model/FrameHub.hpp"
#include "model/Recorder.hpp"
#include "view/RecorderWindow.hpp"

#include <nlohmann/json.hpp>

#include <QApplication>

#include <exception>
#include <fstream>

using namespace apps::data_recorder::ui;

namespace
{

bool loadConfig(const std::string& path, UiConfig& config)
{
    std::ifstream stream(path);
    if (!stream.is_open())
    {
        qCCritical(uiLog, "Cannot open node ipc file '%s'", path.c_str());
        return false;
    }

    try
    {
        config = UiConfig::fromJson(nlohmann::json::parse(stream));
    }
    catch (const std::exception& error)
    {
        qCCritical(uiLog, "Invalid node ipc file '%s': %s", path.c_str(), error.what());
        return false;
    }

    return true;
}

} // namespace

int main(int argc, char* argv[])
{
    AppOptions options;
    if (!AppOptions::parse(argc, argv, options))
    {
        return 1;
    }

    UiConfig config;
    if (!loadConfig(options.nodesIpcFile, config))
    {
        return 1;
    }

    QApplication application(argc, argv);

    // Frames cross from the capture threads to the GUI thread through queued
    // connections, which requires the type to be known to the meta-object system.
    qRegisterMetaType<FramePtr>();
    qRegisterMetaType<DataSource>();

    RecorderConfig recorderConfig;
    recorderConfig.encoderType = options.encoderType;
    recorderConfig.g2dType = options.g2dType;
    recorderConfig.muxerType = options.muxerType;
    recorderConfig.outputDir = options.outputDir;
    recorderConfig.fps = config.recordFps;

    try
    {
        RecorderWindow window(config);
        FrameHub hub(config);
        Recorder recorder(recorderConfig);
        GuiController controller(config, window, hub, recorder);

        window.show();
        hub.start();

        const int result = application.exec();

        // Stop the capture threads before the window and models unwind.
        hub.stop();
        return result;
    }
    catch (const std::exception& error)
    {
        qCCritical(uiLog, "Fatal error: %s", error.what());
        return 1;
    }
}
