#ifndef __GUI_CLIENT_HPP__
#define __GUI_CLIENT_HPP__

#include <QApplication>
#include "VideoFrameWidget.h"
#include <common/ImageFrameAdapter.hpp>

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <thread>


using json = nlohmann::json;
using namespace midware::zeromq_ipc;
using namespace apps::data_recorder;

namespace apps
{
namespace data_recorder
{
namespace ui
{

class GuiClient
{
public:
    GuiClient(int argc, char *argv[], const json& gui_ipc, const std::string& g2dPlatform);
    ~GuiClient();

    void runLoop();

private:
    void installGuiUpdateCallback();

    void installFlipFrameCallbackCallback(std::shared_ptr<ImageFrameAdapter> data_adapter);

private:
    std::unique_ptr<QApplication> m_app;
    std::unique_ptr<VideoFrameWidget> m_video_frame_widget;

    std::unordered_map<std::string, std::shared_ptr<ImageFrameAdapter>> m_data_adapters;
    std::vector<std::thread> m_data_adapter_threads;

};

} // namespace ui
} // namespace data_recorder
} // namespace apps


#endif // __GUI_CLIENT_HPP__
