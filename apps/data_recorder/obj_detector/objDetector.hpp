#ifndef __OBJ_DETECTOR_HPP__
#define __OBJ_DETECTOR_HPP__

#include <string>
#include <nlohmann/json.hpp>
#include <zeromq_ipc/sharedMemSubscriber.hpp>
#include <zeromq_ipc/sharedMemPublisher.hpp>
#include <shared/BspLogger.hpp>
#include <shared/ArgParser.hpp>
#include <bsp_dnn/dnnObjDetector.hpp>
#include <bsp_codec/IDecoder.hpp>
#include <atomic>
#include <mutex>
#include <queue>

using json = nlohmann::json;
using namespace midware::zeromq_ipc;
using namespace bsp_perf::shared;

namespace apps
{
namespace data_recorder
{

class ObjDetector
{
public:
    ObjDetector(ArgParser&& args, const json& nodes_ipc);

    void runLoop();

    ~ObjDetector();

private:
    void inferenceLoop();

    void setObjDetectParams(ArgParser& args, std::unique_ptr<bsp_dnn::dnnObjDetector>& dnnObjDetector);

    std::vector<bsp_dnn::ObjDetectOutputBox>& runDnnInference(std::shared_ptr<bsp_codec::DecodeOutFrame> frame);

    void publishObjDetectResults(std::vector<bsp_dnn::ObjDetectOutputBox>& output_boxes, std::shared_ptr<bsp_codec::DecodeOutFrame> frame);

private:
    std::string m_name;
    std::shared_ptr<SharedMemSubscriber> m_input_shmem_port;
    std::shared_ptr<SharedMemPublisher> m_output_shmem_port;
    std::unique_ptr<bsp_dnn::dnnObjDetector> m_dnnObjDetector;
    bsp_dnn::ObjDetectParams m_objDetectParams{};

    std::queue<std::shared_ptr<bsp_codec::DecodeOutFrame>> m_free_frames_queue;
    std::mutex m_free_frames_queue_mutex;
    const size_t m_free_frames_queue_size{30};

    std::queue<std::shared_ptr<bsp_codec::DecodeOutFrame>> m_inference_frames_queue;
    std::mutex m_inference_frames_queue_mutex;
    const size_t m_inference_frames_queue_size{30};

    std::atomic<bool> m_stopSignal{false};

    std::unique_ptr<std::thread> m_inference_thread{nullptr};
};

} // namespace data_recorder
} // namespace apps

#endif