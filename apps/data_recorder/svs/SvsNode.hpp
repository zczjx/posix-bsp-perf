#ifndef __SVS_NODE_HPP__
#define __SVS_NODE_HPP__

#include <array>
#include <atomic>
#include <bsp_image/ImageBuffer.hpp>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <svs/ISurroundView.hpp>
#include <thread>
#include <vector>
#include <zeromq_ipc/sharedMemPublisher.hpp>
#include <zeromq_ipc/sharedMemSubscriber.hpp>

namespace apps
{
namespace data_recorder
{

class SvsNode
{
public:
    using json = nlohmann::json;

    SvsNode(const json& nodesIpc, const json& vehicleRig, const std::string& dataRoot);
    ~SvsNode();

    void runLoop();

private:
    struct CameraInput
    {
        std::string name;
        std::shared_ptr<midware::zeromq_ipc::SharedMemSubscriber> subscriber;
        std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> latestFrame;
        std::mutex frameMutex;
        uint64_t sequence{0};
    };

    void setup(const json& node, const json& nodesIpc, const json& vehicleRig,
               const std::string& dataRoot);
    void setupIpc(const json& node, const json& nodesIpc);
    bsp_perf::svs::SurroundViewConfig loadAlgorithmConfig(
        const json& node, const json& vehicleRig, const std::string& dataRoot) const;
    void cameraReceiveLoop(size_t cameraIndex);
    void processLoop();
    void publishFrame(const bsp_perf::bsp_image::ImageBuffer& frame);

    static json findNamedNode(const json& array, const std::string& name,
                              const std::string& description);
    static std::string joinPath(const std::string& root, const std::string& path);

private:
    std::string m_name;
    std::string m_backend{"geometry_bev"};
    std::string m_outputPixelFormat{"RGB888"};
    std::shared_ptr<midware::zeromq_ipc::SharedMemPublisher> m_outputPublisher;
    std::array<CameraInput, bsp_perf::svs::kCameraCount> m_cameraInputs;
    std::unique_ptr<bsp_perf::svs::ISurroundView> m_surroundView;
    std::array<uint64_t, bsp_perf::svs::kCameraCount> m_lastProcessedSequence{};
    std::atomic<bool> m_stopSignal{false};
    std::vector<std::thread> m_cameraThreads;
    std::thread m_processThread;
};

} // namespace data_recorder
} // namespace apps

#endif // __SVS_NODE_HPP__
