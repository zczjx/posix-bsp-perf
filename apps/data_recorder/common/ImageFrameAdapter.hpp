#ifndef __IMAGE_FRAME_ADAPTER_HPP__
#define __IMAGE_FRAME_ADAPTER_HPP__

#include <zeromq_ipc/zmqSubscriber.hpp>
#include <bsp_g2d/IGraphics2D.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <memory>
#include <atomic>

using json = nlohmann::json;
using namespace midware::zeromq_ipc;
using namespace bsp_g2d;

namespace apps
{
namespace data_recorder
{
class ImageFrameAdapter
{
public:

    using FlipFrameCallback = std::function<void(const uint8_t* data, int width, int height)>;


    ImageFrameAdapter(const json& data_source_config, const std::string& g2dPlatform);
    virtual ~ImageFrameAdapter() = default;

    void runLoop();

    void setFlipFrameCallback(FlipFrameCallback callback)
    {
        m_flip_frame_callback = callback;
    }

    void stop()
    {
        m_stopSignal.store(true);
    }

    const std::string& getName() const
    {
        return m_name;
    }

    const std::string& getType() const
    {
        return m_type;
    }

private:
    bool needPixelConverter();

private:

    std::string m_name;
    std::string m_type;
    std::string m_info_topic;
    std::string m_data_topic;

    size_t m_image_size_x;
    size_t m_image_size_y;

    std::string m_pixel_format;

    std::shared_ptr<ZmqSubscriber> m_sub_info;
    std::shared_ptr<ZmqSubscriber> m_sub_data;

    std::atomic<bool> m_stopSignal{false};

    std::unique_ptr<IGraphics2D> m_g2d{nullptr};
    std::shared_ptr<IGraphics2D::G2DBuffer> m_g2d_in_buf{nullptr};
    std::shared_ptr<IGraphics2D::G2DBuffer> m_g2d_out_buf{nullptr};

    std::vector<uint8_t> m_in_buffer;
    std::vector<uint8_t> m_out_buffer;

    FlipFrameCallback m_flip_frame_callback{nullptr};

};

} // namespace data_recorder
} // namespace apps

#endif // __IMAGE_FRAME_ADAPTER_HPP__