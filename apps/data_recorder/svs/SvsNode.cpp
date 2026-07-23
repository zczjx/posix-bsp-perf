#include "SvsNode.hpp"

#include <algorithm>
#include <chrono>
#include <common/msg/CameraSensorMsg.hpp>
#include <cstring>
#include <iostream>
#include <msgpack.hpp>
#include <stdexcept>

namespace apps
{
namespace data_recorder
{
namespace
{
constexpr size_t kMaxMetadataSize = 4096;

void loadPose(const nlohmann::json& source, bsp_perf::svs::CameraPose& pose)
{
    pose.x = source.value("x", 0.0);
    pose.y = source.value("y", 0.0);
    pose.z = source.value("z", 0.0);
    pose.roll = source.value("roll", 0.0);
    pose.pitch = source.value("pitch", 0.0);
    pose.yaw = source.value("yaw", 0.0);
}

size_t bytesPerPixel(const std::string& pixelFormat)
{
    if (pixelFormat == "RGB888" || pixelFormat == "BGR888") {
        return 3;
    }
    if (pixelFormat == "RGBA8888" || pixelFormat == "BGRA8888") {
        return 4;
    }
    return 0;
}
} // namespace

SvsNode::SvsNode(const json& nodesIpc, const json& vehicleRig, const std::string& dataRoot)
{
    if (!nodesIpc.contains("svs") || !nodesIpc["svs"].is_array() ||
        nodesIpc["svs"].empty()) {
        throw std::runtime_error("nodes_ipc missing svs configuration");
    }

    setup(nodesIpc["svs"][0], nodesIpc, vehicleRig, dataRoot);
    for (size_t i = 0; i < bsp_perf::svs::kCameraCount; ++i) {
        m_cameraThreads.emplace_back([this, i]() { cameraReceiveLoop(i); });
    }
    m_processThread = std::thread([this]() { processLoop(); });
}

SvsNode::~SvsNode()
{
    m_stopSignal.store(true);

    if (m_processThread.joinable()) {
        m_processThread.join();
    }
    for (auto& thread : m_cameraThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    if (m_surroundView) {
        m_surroundView->tearDown();
    }
}

void SvsNode::runLoop()
{
    while (!m_stopSignal.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void SvsNode::setup(const json& node,
                    const json& nodesIpc,
                    const json& vehicleRig,
                    const std::string& dataRoot)
{
    m_name = node.value("name", std::string("svs"));
    m_backend = node.value("backend", std::string("geometry_bev"));
    m_outputPixelFormat = node.value("output_pixel_format", std::string("RGB888"));
    if (m_outputPixelFormat != "RGB888") {
        throw std::runtime_error("svs output_pixel_format must be RGB888");
    }

    setupIpc(node, nodesIpc);
    const auto config = loadAlgorithmConfig(node, vehicleRig, dataRoot);
    const size_t outputSize = static_cast<size_t>(config.outputWidth) *
                              static_cast<size_t>(config.outputHeight) * 3U;
    if (outputSize > m_outputPublisher->getSingleBufferMaxSize()) {
        throw std::runtime_error("svs publisher shared-memory buffer is smaller than configured output");
    }

    m_surroundView = bsp_perf::svs::ISurroundView::create(m_backend);
    if (m_surroundView->setup(config) != 0) {
        throw std::runtime_error("failed to setup surround view backend: " + m_backend);
    }
}

void SvsNode::setupIpc(const json& node, const json& nodesIpc)
{
    if (!node.contains("publisher") || !node["publisher"].is_object()) {
        throw std::runtime_error("svs node missing publisher configuration");
    }
    if (!node.contains("input_cameras") || !node["input_cameras"].is_array() ||
        node["input_cameras"].size() != bsp_perf::svs::kCameraCount) {
        throw std::runtime_error("svs.input_cameras must list exactly 4 camera names");
    }
    if (!nodesIpc.contains("camera") || !nodesIpc["camera"].is_array()) {
        throw std::runtime_error("nodes_ipc missing camera array");
    }

    const auto& publisher = node["publisher"];
    m_outputPublisher = std::make_shared<midware::zeromq_ipc::SharedMemPublisher>(
        publisher.at("topic").get<std::string>(),
        publisher.at("shmem").get<std::string>(),
        publisher.at("shmem_slots").get<size_t>(),
        publisher.at("shmem_single_buffer_size").get<size_t>());

    for (size_t i = 0; i < bsp_perf::svs::kCameraCount; ++i) {
        const std::string cameraName = node["input_cameras"][i].get<std::string>();
        const json cameraNode = findNamedNode(
            nodesIpc["camera"], cameraName, "camera node");
        if (!cameraNode.contains("publisher") || !cameraNode["publisher"].is_object()) {
            throw std::runtime_error("camera node missing publisher: " + cameraName);
        }

        const auto& cameraPublisher = cameraNode["publisher"];
        auto& input = m_cameraInputs[i];
        input.name = cameraName;
        input.subscriber = std::make_shared<midware::zeromq_ipc::SharedMemSubscriber>(
            cameraPublisher.at("topic").get<std::string>(),
            cameraPublisher.at("shmem").get<std::string>(),
            cameraPublisher.at("shmem_slots").get<size_t>(),
            cameraPublisher.at("shmem_single_buffer_size").get<size_t>());
    }
}

bsp_perf::svs::SurroundViewConfig SvsNode::loadAlgorithmConfig(
    const json& node,
    const json& vehicleRig,
    const std::string& dataRoot) const
{
    if (!vehicleRig.contains("sensors") || !vehicleRig["sensors"].is_array()) {
        throw std::runtime_error("vehicle rig missing sensors array");
    }

    bsp_perf::svs::SurroundViewConfig config;
    if (node.contains("output_size") && node["output_size"].is_array() &&
        node["output_size"].size() == 2) {
        config.outputWidth = node["output_size"][0].get<uint32_t>();
        config.outputHeight = node["output_size"][1].get<uint32_t>();
    }
    config.groundZ = node.value("ground_z", config.groundZ);
    config.featherPower = node.value("feather_power", config.featherPower);
    config.fillUncovered = node.value("fill_uncovered", config.fillUncovered);
    config.inpaintRadius = node.value("inpaint_radius", config.inpaintRadius);
    config.vehicleImagePath = joinPath(
        dataRoot, node.value("vehicle_image", std::string()));

    if (node.contains("ground_extent_m")) {
        const auto& extent = node["ground_extent_m"];
        config.groundExtent.front = extent.value("front", config.groundExtent.front);
        config.groundExtent.rear = extent.value("rear", config.groundExtent.rear);
        config.groundExtent.left = extent.value("left", config.groundExtent.left);
        config.groundExtent.right = extent.value("right", config.groundExtent.right);
    }
    if (node.contains("vehicle_size_m")) {
        const auto& vehicle = node["vehicle_size_m"];
        config.vehicleSize.length = vehicle.value("length", config.vehicleSize.length);
        config.vehicleSize.width = vehicle.value("width", config.vehicleSize.width);
    }
    if (node.contains("projection_region_m")) {
        const auto& region = node["projection_region_m"];
        config.projectionRegion.nearDistance =
            region.value("near_distance", config.projectionRegion.nearDistance);
        config.projectionRegion.nearHalfWidth =
            region.value("near_half_width", config.projectionRegion.nearHalfWidth);
        config.projectionRegion.farDistance =
            region.value("far_distance", config.projectionRegion.farDistance);
        config.projectionRegion.farHalfWidth =
            region.value("far_half_width", config.projectionRegion.farHalfWidth);
    }

    for (size_t i = 0; i < bsp_perf::svs::kCameraCount; ++i) {
        const std::string& cameraName = m_cameraInputs[i].name;
        const json sensor = findNamedNode(
            vehicleRig["sensors"], cameraName, "vehicle rig camera");
        if (!sensor.contains("spawn_point") || !sensor["spawn_point"].is_object()) {
            throw std::runtime_error("vehicle rig camera missing spawn_point: " + cameraName);
        }

        config.cameras[i].name = cameraName;
        config.cameras[i].fov = sensor.value("fov", 90.0);
        loadPose(sensor["spawn_point"], config.cameras[i].pose);
    }
    return config;
}

void SvsNode::cameraReceiveLoop(size_t cameraIndex)
{
    auto metadata = std::shared_ptr<uint8_t[]>(new uint8_t[kMaxMetadataSize]);
    std::shared_ptr<uint8_t[]> storage;
    size_t storageCapacity = 0;
    auto& input = m_cameraInputs[cameraIndex];

    while (!m_stopSignal.load()) {
        const size_t metadataSize = input.subscriber->receiveMsg(metadata, kMaxMetadataSize);
        if (metadataSize == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        try {
            msgpack::unpacked unpacked = msgpack::unpack(
                reinterpret_cast<const char*>(metadata.get()), metadataSize);
            const CameraSensorMsg message = unpacked.get().as<CameraSensorMsg>();
            const size_t pixelSize = bytesPerPixel(message.pixel_format);
            const size_t minimumDataSize =
                static_cast<size_t>(std::max(message.width, 0)) *
                static_cast<size_t>(std::max(message.height, 0)) * pixelSize;
            if (message.width <= 0 || message.height <= 0 || pixelSize == 0 ||
                message.data_size < minimumDataSize ||
                message.data_size % static_cast<size_t>(message.height) != 0) {
                continue;
            }

            if (!storage || storageCapacity < message.data_size || storage.use_count() > 1) {
                storage = std::shared_ptr<uint8_t[]>(new uint8_t[message.data_size]);
                storageCapacity = message.data_size;
            }
            const int received = input.subscriber->receiveSharedMemData(
                storage.get(), message.data_size, message.slot_index);
            if (received < 0 || static_cast<size_t>(received) != message.data_size) {
                continue;
            }

            bsp_perf::bsp_image::ImageDesc desc{};
            desc.width = static_cast<uint32_t>(message.width);
            desc.height = static_cast<uint32_t>(message.height);
            desc.widthStride = desc.width;
            desc.heightStride = desc.height;
            desc.format = message.pixel_format;
            desc.dataSize = message.data_size;
            const size_t rowStride = message.data_size / static_cast<size_t>(message.height);

            auto frame = std::make_shared<bsp_perf::bsp_image::ImageBuffer>();
            frame->owner = storage;
            frame->view = bsp_perf::bsp_image::makeHostImageView(
                storage.get(), desc, static_cast<uint32_t>(rowStride));

            {
                std::lock_guard<std::mutex> lock(input.frameMutex);
                input.latestFrame = std::move(frame);
                ++input.sequence;
            }
        } catch (const std::exception& e) {
            std::cerr << "SvsNode camera input " << input.name
                      << " failed: " << e.what() << std::endl;
        }
    }
}

void SvsNode::processLoop()
{
    bsp_perf::svs::FrameSet inputFrames;
    bsp_perf::svs::OutputFrame output;
    std::array<std::shared_ptr<bsp_perf::bsp_image::ImageBuffer>,
               bsp_perf::svs::kCameraCount>
        snapshots;
    size_t frameCount = 0;
    auto lastReport = std::chrono::steady_clock::now();

    while (!m_stopSignal.load()) {
        bool ready = true;
        std::array<uint64_t, bsp_perf::svs::kCameraCount> sequences{};
        for (size_t i = 0; i < bsp_perf::svs::kCameraCount; ++i) {
            std::lock_guard<std::mutex> lock(m_cameraInputs[i].frameMutex);
            if (!m_cameraInputs[i].latestFrame ||
                m_cameraInputs[i].sequence == m_lastProcessedSequence[i]) {
                ready = false;
                break;
            }
            snapshots[i] = m_cameraInputs[i].latestFrame;
            sequences[i] = m_cameraInputs[i].sequence;
        }

        if (!ready) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        for (size_t i = 0; i < bsp_perf::svs::kCameraCount; ++i) {
            inputFrames.cameras[i] = snapshots[i]->view;
        }

        if (m_surroundView->process(inputFrames, output) != 0 || output.image.view.empty()) {
            std::cerr << "SvsNode failed to process frame set" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        m_lastProcessedSequence = sequences;
        publishFrame(output.image);
        ++frameCount;

        const auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - lastReport).count() >= 1) {
            std::cout << "SvsNode FPS: " << frameCount << std::endl;
            frameCount = 0;
            lastReport = now;
        }
    }
}

void SvsNode::publishFrame(const bsp_perf::bsp_image::ImageBuffer& frame)
{
    if (frame.view.empty() || frame.view.data() == nullptr ||
        frame.view.desc.dataSize > m_outputPublisher->getSingleBufferMaxSize()) {
        return;
    }

    CameraSensorMsg message;
    message.publisher_id = m_name;
    message.pixel_format = m_outputPixelFormat;
    message.width = static_cast<int>(frame.view.desc.width);
    message.height = static_cast<int>(frame.view.desc.height);
    message.data_size = frame.view.desc.dataSize;
    message.slot_index = m_outputPublisher->getFreeSlotIndex();

    msgpack::sbuffer metadata;
    msgpack::pack(metadata, message);
    if (m_outputPublisher->publishData(
            reinterpret_cast<const uint8_t*>(metadata.data()),
            metadata.size(),
            frame.view.data(),
            message.slot_index,
            message.data_size) < 0) {
        std::cerr << "SvsNode failed to publish output frame" << std::endl;
    }
}

SvsNode::json SvsNode::findNamedNode(const json& array,
                                     const std::string& name,
                                     const std::string& description)
{
    if (!array.is_array()) {
        throw std::runtime_error(description + " collection is not an array");
    }
    for (const auto& item : array) {
        if (item.value("name", std::string()) == name) {
            return item;
        }
    }
    throw std::runtime_error(description + " not found: " + name);
}

std::string SvsNode::joinPath(const std::string& root, const std::string& path)
{
    if (path.empty() || root.empty() || path.front() == '/') {
        return path;
    }
    if (root.back() == '/') {
        return root + path;
    }
    return root + "/" + path;
}

} // namespace data_recorder
} // namespace apps
