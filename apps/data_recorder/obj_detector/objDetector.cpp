#include "objDetector.hpp"
#include <common/msg/ObjDetectMsg.hpp>
#include <thread>
#include <chrono>

namespace apps
{
namespace data_recorder
{

ObjDetector::ObjDetector(ArgParser&& args, const json& nodes_ipc)
{
    for (const auto& node: nodes_ipc["object_detector"])
    {
        m_name = node["name"];
        std::string status{node["status"]};

        const json& subscriber = node["subscriber"];
        const json& publisher = node["publisher"];

        m_input_shmem_port = std::make_shared<SharedMemSubscriber>(subscriber["topic"],
                                                                   subscriber["shmem"],
                                                                   subscriber["shmem_slots"],
                                                                   subscriber["shmem_single_buffer_size"]);

        m_output_shmem_port = std::make_shared<SharedMemPublisher>(publisher["topic"],
                                                                   publisher["shmem"],
                                                                   publisher["shmem_slots"],
                                                                   publisher["shmem_single_buffer_size"]);
    }
    {
        std::string dnnType;
        std::string pluginPath;
        std::string labelTextPath;
        std::string modelPath;
        args.getOptionVal("--dnnType", dnnType);
        args.getOptionVal("--pluginPath", pluginPath);
        args.getOptionVal("--labelTextPath", labelTextPath);
        args.getOptionVal("--modelPath", modelPath);
        m_dnnObjDetector = std::make_unique<bsp_dnn::dnnObjDetector>(dnnType, pluginPath, labelTextPath);
        m_dnnObjDetector->loadModel(modelPath);
        setObjDetectParams(args, m_dnnObjDetector);
    }
    m_inference_thread = std::make_unique<std::thread>([this]() {inferenceLoop();});
}

ObjDetector::~ObjDetector()
{
    if (m_inference_thread->joinable())
    {
        m_stopSignal.store(true);
        m_inference_thread->join();
    }
    m_dnnObjDetector.reset();
    m_input_shmem_port.reset();
    m_output_shmem_port.reset();
}

void ObjDetector::setObjDetectParams(ArgParser& args, std::unique_ptr<bsp_dnn::dnnObjDetector>& dnnObjDetector)
{
    bsp_dnn::IDnnEngine::dnnInputShape shape;
    dnnObjDetector->getInputShape(shape);
    m_objDetectParams.model_input_width = shape.width;
    m_objDetectParams.model_input_height = shape.height;
    m_objDetectParams.model_input_channel = shape.channel;
    args.getSubOptionVal("objDetectParams", "--conf_threshold", m_objDetectParams.conf_threshold);
    args.getSubOptionVal("objDetectParams", "--nms_threshold", m_objDetectParams.nms_threshold);
    args.getSubOptionVal("objDetectParams", "--pads_left", m_objDetectParams.pads.left);
    args.getSubOptionVal("objDetectParams", "--pads_right", m_objDetectParams.pads.right);
    args.getSubOptionVal("objDetectParams", "--pads_top", m_objDetectParams.pads.top);
    args.getSubOptionVal("objDetectParams", "--pads_bottom", m_objDetectParams.pads.bottom);
    dnnObjDetector->getOutputQuantParams(m_objDetectParams.quantize_zero_points, m_objDetectParams.quantize_scales);
}

std::vector<bsp_dnn::ObjDetectOutputBox>& ObjDetector::runDnnInference(std::shared_ptr<bsp_codec::DecodeOutFrame> frame)
{
    bsp_dnn::ObjDetectInput objDetectInput = {
        .handleType = "DecodeOutFrame",
        .imageHandle = frame,
    };
    m_dnnObjDetector->pushInputData(std::make_shared<bsp_dnn::ObjDetectInput>(objDetectInput));
    m_objDetectParams.scale_width = static_cast<float>(m_objDetectParams.model_input_width) / static_cast<float>(frame->width);
    m_objDetectParams.scale_height = static_cast<float>(m_objDetectParams.model_input_height) / static_cast<float>(frame->height);
    m_dnnObjDetector->runObjDetect(m_objDetectParams);
    return m_dnnObjDetector->popOutputData();
}

void ObjDetector::publishObjDetectResults(std::vector<bsp_dnn::ObjDetectOutputBox>& output_boxes, std::shared_ptr<bsp_codec::DecodeOutFrame> frame)
{
    msgpack::sbuffer msg_buffer;
    ObjDetectMsg objDetectMsg;
    objDetectMsg.publisher_id = m_name;
    objDetectMsg.original_frame.publisher_id = m_name;
    objDetectMsg.original_frame.pixel_format = frame->format;
    objDetectMsg.original_frame.width = frame->width;
    objDetectMsg.original_frame.height = frame->height;
    objDetectMsg.original_frame.data_size = frame->valid_data_size;
    objDetectMsg.original_frame.slot_index = m_output_shmem_port->getFreeSlotIndex();
    objDetectMsg.output_boxes = std::vector<bsp_dnn::ObjDetectOutputBox>(output_boxes);
    msgpack::pack(msg_buffer, objDetectMsg);
    m_output_shmem_port->publishData(reinterpret_cast<const uint8_t*>(msg_buffer.data()),
                                            msg_buffer.size(), frame->virt_addr,
                                            objDetectMsg.original_frame.slot_index, objDetectMsg.original_frame.data_size);
}

void ObjDetector::inferenceLoop()
{
    std::shared_ptr<bsp_codec::DecodeOutFrame> inference_frame{nullptr};
    while (!m_stopSignal.load())
    {
        {
            std::lock_guard<std::mutex> lock(m_inference_frames_queue_mutex);
            if (!m_inference_frames_queue.empty())
            {
                inference_frame = m_inference_frames_queue.front();
                m_inference_frames_queue.pop();
            }
        }

        if (inference_frame == nullptr)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        else
        {
            std::vector<bsp_dnn::ObjDetectOutputBox>& output_boxes = runDnnInference(inference_frame);
            for (const auto& output_box : output_boxes)
            {
                std::cout << "ObjDetector::inferenceLoop() output_box: " << output_box.label << std::endl;
            }
            publishObjDetectResults(output_boxes, inference_frame);

            std::lock_guard<std::mutex> lock(m_free_frames_queue_mutex);
            m_free_frames_queue.push(inference_frame);
            inference_frame = nullptr;
        }
    }
}
void ObjDetector::runLoop()
{
    std::shared_ptr<uint8_t[]> msg_buffer(new uint8_t[sizeof(CameraSensorMsg)]);
    std::shared_ptr<uint8_t[]> data_buffer{nullptr};
    std::shared_ptr<bsp_codec::DecodeOutFrame> inference_frame{nullptr};

    while (!m_stopSignal.load())
    {

        size_t msg_size = m_input_shmem_port->receiveMsg(msg_buffer, sizeof(CameraSensorMsg));
        if (msg_size <= 0)
        {
            std::cerr << "ObjDetector::runLoop() receive msg failed" << std::endl;
        }

        msgpack::unpacked result = msgpack::unpack(reinterpret_cast<const char*>(msg_buffer.get()), msg_size);
        CameraSensorMsg shmem_msg = result.get().as<CameraSensorMsg>();

        if(m_free_frames_queue.empty())
        {
            inference_frame = std::make_shared<bsp_codec::DecodeOutFrame>();
            inference_frame->fd = -1;
            inference_frame->virt_addr = new uint8_t[shmem_msg.data_size];
            inference_frame->valid_data_size = shmem_msg.data_size;
            inference_frame->width = shmem_msg.width;
            inference_frame->height = shmem_msg.height;
            inference_frame->width_stride = shmem_msg.width;
            inference_frame->height_stride = shmem_msg.height;
            inference_frame->format = shmem_msg.pixel_format;
        }
        else
        {
            std::lock_guard<std::mutex> lock(m_free_frames_queue_mutex);
            inference_frame = m_free_frames_queue.front();
            m_free_frames_queue.pop();
        }

        m_input_shmem_port->receiveSharedMemData(inference_frame->virt_addr, shmem_msg.data_size, shmem_msg.slot_index);

        {
            std::lock_guard<std::mutex> lock(m_inference_frames_queue_mutex);
            m_inference_frames_queue.push(inference_frame);
            while (m_inference_frames_queue.size() > m_inference_frames_queue_size)
            {
                m_inference_frames_queue.pop();
            }
        }

        while(m_free_frames_queue.size() >= m_free_frames_queue_size)
        {
            std::lock_guard<std::mutex> lock(m_free_frames_queue_mutex);
            m_free_frames_queue.pop();
        }
    }
}

} // namespace data_recorder
} // namespace apps