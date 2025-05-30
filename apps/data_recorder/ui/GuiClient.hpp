#ifndef __GUI_CLIENT_HPP__
#define __GUI_CLIENT_HPP__

#include <QApplication>
#include "VideoFrameWidget.h"
#include "DataSource.hpp"

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <thread>


using json = nlohmann::json;
using namespace midware::zeromq_ipc;

namespace apps
{
namespace data_recorder
{

class GuiClient
{
public:
    GuiClient(int argc, char *argv[], const json& gui_ipc, const std::string& g2dPlatform);
    ~GuiClient();

    void runLoop();

private:
    void installGuiUpdateCallback();

    void installFlipFrameCallbackCallback(std::shared_ptr<DataSource> data_source);

private:
    std::unique_ptr<QApplication> m_app;
    std::unique_ptr<VideoFrameWidget> m_video_frame_widget;

    std::unordered_map<std::string, std::shared_ptr<DataSource>> m_data_sources;
    std::vector<std::thread> m_data_source_threads;


};

} // namespace data_recorder
} // namespace apps

#endif // __GUI_CLIENT_HPP__
