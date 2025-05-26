
#include "GuiClient.hpp"

namespace apps
{
namespace data_recorder
{

GuiClient::GuiClient(int argc, char *argv[], const json& gui_ipc):
    m_app(std::make_unique<QApplication>(argc, argv)),
    m_video_frame_widget(std::make_unique<VideoFrameWidget>())
{
}

void GuiClient::runLoop()
{
    m_app->exec();
}

GuiClient::~GuiClient()
{
    m_app->quit();
}


} // namespace data_recorder
} // namespace apps
