#include "objDetector.hpp"
#include <common/msg/ObjDetectMsg.hpp>
#include <common/msg/CameraSensorMsg.hpp>

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
        std::string name{node["name"]};
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
        setObjDetectParams(args);
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

void ObjDetector::setObjDetectParams(ArgParser& args)
{
    bsp_dnn::IDnnEngine::dnnInputShape shape;
    m_dnnObjDetector->getInputShape(shape);
    m_objDetectParams.model_input_width = shape.width;
    m_objDetectParams.model_input_height = shape.height;
    m_objDetectParams.model_input_channel = shape.channel;
    args.getSubOptionVal("objDetectParams", "--conf_threshold", m_objDetectParams.conf_threshold);
    args.getSubOptionVal("objDetectParams", "--nms_threshold", m_objDetectParams.nms_threshold);
    m_dnnObjDetector->getOutputQuantParams(m_objDetectParams.quantize_zero_points, m_objDetectParams.quantize_scales);
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
            bsp_dnn::ObjDetectInput objDetectInput = {
                .handleType = "DecodeOutFrame",
                .imageHandle = inference_frame,
            };
            m_dnnObjDetector->pushInputData(std::make_shared<bsp_dnn::ObjDetectInput>(objDetectInput));
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
    }
}

} // namespace data_recorder
} // namespace apps