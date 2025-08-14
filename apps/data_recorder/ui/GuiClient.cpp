#include "GuiClient.hpp"
#include <common/msg/CameraSensorMsg.hpp>
#include <common/msg/ObjDetectMsg.hpp>
#include <iostream>
#include <thread>
#include <opencv2/opencv.hpp>

namespace apps
{
namespace data_recorder
{
namespace ui
{
GuiClient::GuiClient(int argc, char *argv[], const json& gui_ipc):
    m_app(std::make_unique<QApplication>(argc, argv)),
    m_video_frame_widget(std::make_unique<VideoFrameWidget>())
{
    m_video_frame_widget->show();

    setupCameraConsumer(gui_ipc);
    setupObjectDetectionConsumer(gui_ipc);

}

void GuiClient::setupCameraConsumer(const json& gui_ipc)
{
    for (const auto& sensor: gui_ipc["camera"])
    {
        std::string sensor_type = sensor["type"];
        const json& publisher = sensor["publisher"];
        m_input_shmem_ports[sensor["name"]] = std::make_pair(sensor_type, std::make_shared<SharedMemSubscriber>(publisher["topic"], publisher["shmem"], publisher["shmem_slots"], publisher["shmem_single_buffer_size"]));
    }

    for (const auto& input_pair: m_input_shmem_ports)
    {
        std::string sensor_type = input_pair.second.first;

        if (sensor_type.compare("camera") == 0)
        {
            m_input_shmem_threads.push_back(std::thread([this, input_pair]() {
                CameraConsumerLoop(input_pair.second.second);
            }));
        }
    }

}

void GuiClient::setupObjectDetectionConsumer(const json& gui_ipc)
{
    for (const auto& sensor: gui_ipc["object_detector"])
    {
        std::string sensor_type = sensor["type"];
        const json& publisher = sensor["publisher"];
        m_input_shmem_ports[sensor["name"]] = std::make_pair(sensor_type, std::make_shared<SharedMemSubscriber>(publisher["topic"], publisher["shmem"], publisher["shmem_slots"], publisher["shmem_single_buffer_size"]));
    }

    for (const auto& input_pair: m_input_shmem_ports)
    {
        std::string sensor_type = input_pair.second.first;

        if (sensor_type.compare("object_detector") == 0)
        {
            m_input_shmem_threads.push_back(std::thread([this, input_pair]() {
                ObjectDetectionConsumerLoop(input_pair.second.second);
            }));
        }
    }
}

void GuiClient::CameraConsumerLoop(std::shared_ptr<SharedMemSubscriber> input_shmem_port)
{

    std::shared_ptr<uint8_t[]> msg_buffer(new uint8_t[sizeof(CameraSensorMsg)]);
    std::shared_ptr<uint8_t[]> data_buffer{nullptr};

    while (!m_stopSignal.load())
    {
        size_t msg_size = input_shmem_port->receiveMsg(msg_buffer, sizeof(CameraSensorMsg));
        if (msg_size <= 0)
        {
            std::cerr << "GuiClient::CameraConsumerLoop() receive msg failed" << std::endl;
            continue;
        }

        if (m_video_frame_widget->getCurrentDataSource() != VideoFrameWidget::RawCamera)
        {
            continue;
        }

        std::cout << "GuiClient::CameraConsumerLoop() process msg" << std::endl;
        msgpack::unpacked result = msgpack::unpack(reinterpret_cast<const char*>(msg_buffer.get()), msg_size);
        CameraSensorMsg shmem_msg = result.get().as<CameraSensorMsg>();

        if (data_buffer == nullptr)
        {
            data_buffer.reset(new uint8_t[shmem_msg.data_size]);
        }

        input_shmem_port->receiveSharedMemData(data_buffer, shmem_msg.data_size, shmem_msg.slot_index);

        if(shmem_msg.pixel_format.compare("RGB888") == 0)
        {
            m_video_frame_widget->setFrame(data_buffer.get(), shmem_msg.width, shmem_msg.height);
        }
        else
        {
            std::cerr << "GuiClient::CameraConsumerLoop() unsupported pixel format: " << shmem_msg.pixel_format << std::endl;
        }
    }
}

void GuiClient::ObjectDetectionConsumerLoop(std::shared_ptr<SharedMemSubscriber> input_shmem_port)
{

    std::shared_ptr<uint8_t[]> msg_buffer(new uint8_t[sizeof(ObjDetectMsg)]);
    std::shared_ptr<uint8_t[]> data_buffer{nullptr};

    while (!m_stopSignal.load())
    {
        size_t msg_size = input_shmem_port->receiveMsg(msg_buffer, sizeof(ObjDetectMsg));
        if (msg_size <= 0)
        {
            std::cerr << "GuiClient::CameraConsumerLoop() receive msg failed" << std::endl;
            continue;
        }

        if (m_video_frame_widget->getCurrentDataSource() != VideoFrameWidget::ObjectsDetection)
        {
            continue;
        }

        std::cout << "GuiClient::ObjectDetectionConsumerLoop() process msg" << std::endl;
        msgpack::unpacked result = msgpack::unpack(reinterpret_cast<const char*>(msg_buffer.get()), msg_size);
        ObjDetectMsg shmem_msg = result.get().as<ObjDetectMsg>();

        std::cout << "GuiClient::ObjectDetectionConsumerLoop() valid_box_count: " << shmem_msg.valid_box_count << std::endl;

        if (data_buffer == nullptr)
        {
            data_buffer.reset(new uint8_t[shmem_msg.original_frame.data_size]);
        }

        input_shmem_port->receiveSharedMemData(data_buffer, shmem_msg.original_frame.data_size, shmem_msg.original_frame.slot_index);

        if(shmem_msg.original_frame.pixel_format.compare("RGB888") == 0)
        {
            cv::Mat cvRGB888Image(shmem_msg.original_frame.height, shmem_msg.original_frame.width, CV_8UC3, data_buffer.get());

            for (size_t i = 0; i < shmem_msg.valid_box_count; i++)
            {
                std::cout << "GuiClient::ObjectDetectionConsumerLoop() output_boxes[" << i << "].label: " << shmem_msg.output_boxes[i].label << std::endl;
                std::cout << "GuiClient::ObjectDetectionConsumerLoop() output_boxes[" << i << "].bbox.left: " << shmem_msg.output_boxes[i].bbox.left << std::endl;
                std::cout << "GuiClient::ObjectDetectionConsumerLoop() output_boxes[" << i << "].bbox.top: " << shmem_msg.output_boxes[i].bbox.top << std::endl;
                std::cout << "GuiClient::ObjectDetectionConsumerLoop() output_boxes[" << i << "].bbox.right: " << shmem_msg.output_boxes[i].bbox.right << std::endl;
                std::cout << "GuiClient::ObjectDetectionConsumerLoop() output_boxes[" << i << "].bbox.bottom: " << shmem_msg.output_boxes[i].bbox.bottom << std::endl;

                cv::rectangle(cvRGB888Image,
                    cv::Point(shmem_msg.output_boxes[i].bbox.left, shmem_msg.output_boxes[i].bbox.top),
                    cv::Point(shmem_msg.output_boxes[i].bbox.right, shmem_msg.output_boxes[i].bbox.bottom),
                    cv::Scalar(255, 0, 0), 2);
                cv::putText(cvRGB888Image, shmem_msg.output_boxes[i].label,
                    cv::Point(shmem_msg.output_boxes[i].bbox.left, shmem_msg.output_boxes[i].bbox.top + 12),
                    cv::FONT_HERSHEY_COMPLEX, 0.4, cv::Scalar(255, 255, 255));
            }
            m_video_frame_widget->setFrame(data_buffer.get(), shmem_msg.original_frame.width, shmem_msg.original_frame.height);
        }
        else
        {
            std::cerr << "GuiClient::CameraConsumerLoop() unsupported pixel format: " << shmem_msg.original_frame.pixel_format << std::endl;
        }
    }
}

void GuiClient::runLoop()
{
    m_app->exec();
}

GuiClient::~GuiClient()
{
    m_stopSignal.store(true);

    for (auto& thread: m_input_shmem_threads)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }

    m_app->quit();
}

} // namespace ui
} // namespace data_recorder
} // namespace apps
