#ifndef __GUI_CLIENT_HPP__
#define __GUI_CLIENT_HPP__

#include <QApplication>
#include "VideoFrameWidget.h"

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <memory>
#include <zeromq_ipc/zmqSubscriber.hpp>


using json = nlohmann::json;
using namespace midware::zeromq_ipc;

namespace apps
{
namespace data_recorder
{

class GuiClient
{
public:
    GuiClient(int argc, char *argv[], const json& gui_ipc);
    ~GuiClient();

    void runLoop();

private:
    std::unique_ptr<QApplication> m_app;
    std::unique_ptr<VideoFrameWidget> m_video_frame_widget;

};

} // namespace data_recorder
} // namespace apps

#endif // __GUI_CLIENT_HPP__
