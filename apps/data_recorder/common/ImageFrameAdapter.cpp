#include "ImageFrameAdapter.hpp"
#include <bsp_g2d/BytesPerPixel.hpp>
#include <iostream>


namespace apps
{
namespace data_recorder
{

ImageFrameAdapter::ImageFrameAdapter(const json& data_source_config, const std::string& g2dPlatform):
    m_g2d{IGraphics2D::create(g2dPlatform)}
{
    m_name = data_source_config["name"];
    m_type = data_source_config["type"];

    if (m_type == "camera")
    {
        m_image_size_x = data_source_config["image_size_x"];
        m_image_size_y = data_source_config["image_size_y"];
        m_pixel_format = data_source_config["pixel_format"];
    }

    m_info_topic = data_source_config["zmq_pub_info"];
    m_data_topic = data_source_config["zmq_pub_data"];

    m_sub_info = std::make_shared<ZmqSubscriber>(m_info_topic);
    m_sub_data = std::make_shared<ZmqSubscriber>(m_data_topic);
}

bool ImageFrameAdapter::needPixelConverter()
{
    if (m_pixel_format.compare("RGB888") == 0)
    {
        return false;
    }

    return true;
}

void ImageFrameAdapter::runLoop()
{
    size_t min_buffer_bytes = m_image_size_x * m_image_size_y
        * bsp_g2d::BytesPerPixel::getInstance().getBytesPerPixel(m_pixel_format);

    size_t min_buffer_bytes_rgb888 = m_image_size_x * m_image_size_y
        * bsp_g2d::BytesPerPixel::getInstance().getBytesPerPixel("RGB888");

    if (m_in_buffer.size() < min_buffer_bytes)
    {
        m_in_buffer.resize(min_buffer_bytes);
    }

    while (!m_stopSignal.load())
    {
        size_t bytes_received = m_sub_data->receiveData(m_in_buffer.data(), m_in_buffer.size());
        if (bytes_received <= 0)
        {
            std::cerr << "ImageFrameAdapter::runLoop() " << m_name << " receive data failed" << std::endl;
            continue;
        }

        if (needPixelConverter())
        {
            if (m_out_buffer.size() < min_buffer_bytes_rgb888)
            {
                m_out_buffer.resize(min_buffer_bytes_rgb888);
            }

            if (m_g2d_in_buf == nullptr)
            {
                IGraphics2D::G2DBufferParams in_g2d_params = {
                    .virtual_addr = m_in_buffer.data(),
                    .rawBufferSize = m_in_buffer.size(),
                    .width = m_image_size_x,
                    .height = m_image_size_y,
                    .width_stride = m_image_size_x,
                    .height_stride = m_image_size_y,
                    .format = m_pixel_format,
                };
                m_g2d_in_buf = m_g2d->createG2DBuffer("virtualaddr", in_g2d_params);
            }

            if (m_g2d_out_buf == nullptr)
            {
                IGraphics2D::G2DBufferParams out_g2d_params = {
                    .virtual_addr = m_out_buffer.data(),
                    .rawBufferSize = m_out_buffer.size(),
                    .width = m_image_size_x,
                    .height = m_image_size_y,
                    .width_stride = m_image_size_x,
                    .height_stride = m_image_size_y,
                    .format = "RGB888",
                };
                m_g2d_out_buf = m_g2d->createG2DBuffer("virtualaddr", out_g2d_params);
            }

            int ret = m_g2d->imageCvtColor(m_g2d_in_buf, m_g2d_out_buf, m_pixel_format, "RGB888");
            if (ret != 1)
            {
                std::cerr << "ImageFrameAdapter::runLoop() " << m_name << " imageCvtColor failed" <<  "ret: " << ret << std::endl;
                continue;
            }

            if (m_flip_frame_callback != nullptr)
            {
                m_flip_frame_callback(m_out_buffer.data(), m_image_size_x, m_image_size_y);
            }
        }
        else
        {
            if (m_flip_frame_callback != nullptr)
            {
                m_flip_frame_callback(m_in_buffer.data(), m_image_size_x, m_image_size_y);
            }
        }
    }

}

} // namespace data_recorder
} // apps