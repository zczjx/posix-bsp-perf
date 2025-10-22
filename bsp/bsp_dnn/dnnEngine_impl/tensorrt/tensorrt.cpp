#include "tensorrt.hpp"
#include <cuda_runtime_api.h>
#include <cassert>
#include <cstring>

namespace bsp_dnn
{

tensorrt::tensorrt() :
    m_logger{std::make_unique<BspLogger>("tensorrt")}
{
    m_logger->printStdoutLog(BspLogger::LogLevel::Info, "TensorRT Engine initialized");
}

tensorrt::~tensorrt()
{
    freeBuffers();

    if (m_params.m_context)
    {
        delete m_params.m_context;
        m_params.m_context = nullptr;
    }

    if (m_params.m_engine)
    {
        delete m_params.m_engine;
        m_params.m_engine = nullptr;
    }

    if (m_params.m_runtime)
    {
        delete m_params.m_runtime;
        m_params.m_runtime = nullptr;
    }

    m_logger->printStdoutLog(BspLogger::LogLevel::Info, "TensorRT Engine destroyed");
}

void tensorrt::loadModel(const std::string& modelPath)
{
    if (modelPath.empty())
    {
        throw std::runtime_error("modelPath is empty.");
    }

    m_logger->printStdoutLog(BspLogger::LogLevel::Info, "Loading model: {}", modelPath);

    // Check file extension
    bool isEngine = (modelPath.substr(modelPath.find_last_of(".") + 1) == "engine");
    bool isOnnx = (modelPath.substr(modelPath.find_last_of(".") + 1) == "onnx");

    bool loadSuccess = false;

    if (isEngine)
    {
        loadSuccess = loadEngineFromFile(modelPath);
    }
    else if (isOnnx)
    {
        loadSuccess = buildEngineFromOnnx(modelPath);
    }
    else
    {
        throw std::runtime_error("Unsupported model format. Use .engine or .onnx");
    }

    if (!loadSuccess)
    {
        throw std::runtime_error("Failed to load model.");
    }

    // Create execution context
    m_params.m_context = m_params.m_engine->createExecutionContext();
    if (!m_params.m_context)
    {
        throw std::runtime_error("Failed to create execution context.");
    }

    // Get input/output information (TensorRT 8.x+ API)
    m_params.m_numInputs = 0;
    m_params.m_numOutputs = 0;

    int32_t numIOTensors = m_params.m_engine->getNbIOTensors();

    for (int32_t i = 0; i < numIOTensors; ++i)
    {
        const char* tensorName = m_params.m_engine->getIOTensorName(i);
        nvinfer1::TensorIOMode ioMode = m_params.m_engine->getTensorIOMode(tensorName);

        if (ioMode == nvinfer1::TensorIOMode::kINPUT)
        {
            m_params.m_inputIndex = i;
            m_params.m_inputName = tensorName;
            m_params.m_inputDims = m_params.m_engine->getTensorShape(tensorName);
            m_params.m_numInputs++;
        }
        else if (ioMode == nvinfer1::TensorIOMode::kOUTPUT)
        {
            m_params.m_outputIndices[m_params.m_numOutputs] = i;
            m_params.m_outputNames.push_back(tensorName);
            m_params.m_outputDims.push_back(m_params.m_engine->getTensorShape(tensorName));
            m_params.m_numOutputs++;
        }
    }

    m_logger->printStdoutLog(BspLogger::LogLevel::Info,
        "Model loaded: {} inputs, {} outputs", m_params.m_numInputs, m_params.m_numOutputs);

    // Allocate CUDA buffers
    allocateBuffers();
}

bool tensorrt::loadEngineFromFile(const std::string& enginePath)
{
    std::ifstream file(enginePath, std::ios::binary);
    if (!file.good())
    {
        m_logger->printStdoutLog(BspLogger::LogLevel::Error, "Failed to open engine file: {}", enginePath);
        return false;
    }

    file.seekg(0, file.end);
    size_t size = file.tellg();
    file.seekg(0, file.beg);

    std::vector<char> engineData(size);
    file.read(engineData.data(), size);
    file.close();

    m_params.m_runtime = nvinfer1::createInferRuntime(m_trtLogger);
    if (!m_params.m_runtime)
    {
        m_logger->printStdoutLog(BspLogger::LogLevel::Error, "Failed to create runtime");
        return false;
    }

    m_params.m_engine = m_params.m_runtime->deserializeCudaEngine(engineData.data(), size);
    if (!m_params.m_engine)
    {
        m_logger->printStdoutLog(BspLogger::LogLevel::Error, "Failed to deserialize engine");
        return false;
    }

    return true;
}

bool tensorrt::buildEngineFromOnnx(const std::string& onnxPath)
{
    m_params.m_runtime = nvinfer1::createInferRuntime(m_trtLogger);
    if (!m_params.m_runtime)
    {
        m_logger->printStdoutLog(BspLogger::LogLevel::Error, "Failed to create runtime");
        return false;
    }

    auto builder = nvinfer1::createInferBuilder(m_trtLogger);
    const uint32_t explicitBatch = 1U << static_cast<uint32_t>(nvinfer1::NetworkDefinitionCreationFlag::kEXPLICIT_BATCH);
    auto network = builder->createNetworkV2(explicitBatch);
    auto parser = nvonnxparser::createParser(*network, m_trtLogger);

    // Parse ONNX
    if (!parser->parseFromFile(onnxPath.c_str(), static_cast<int>(nvinfer1::ILogger::Severity::kWARNING)))
    {
        m_logger->printStdoutLog(BspLogger::LogLevel::Error, "Failed to parse ONNX file");
        return false;
    }

    // Build engine
    auto config = builder->createBuilderConfig();

    // TensorRT 10.x: Use setMemoryPoolLimit instead of setMaxWorkspaceSize
    config->setMemoryPoolLimit(nvinfer1::MemoryPoolType::kWORKSPACE, 1ULL << 30); // 1GB

    // Enable FP16 if supported
    if (builder->platformHasFastFp16())
    {
        config->setFlag(nvinfer1::BuilderFlag::kFP16);
        m_logger->printStdoutLog(BspLogger::LogLevel::Info, "FP16 mode enabled");
    }

    // Build serialized network (TensorRT 10.x)
    nvinfer1::IHostMemory* serializedModel = builder->buildSerializedNetwork(*network, *config);
    if (!serializedModel)
    {
        m_logger->printStdoutLog(BspLogger::LogLevel::Error, "Failed to build serialized network");
        delete parser;
        delete network;
        delete config;
        delete builder;
        return false;
    }

    // Deserialize the engine
    m_params.m_engine = m_params.m_runtime->deserializeCudaEngine(serializedModel->data(), serializedModel->size());

    delete serializedModel;
    delete parser;
    delete network;
    delete config;
    delete builder;

    if (!m_params.m_engine)
    {
        m_logger->printStdoutLog(BspLogger::LogLevel::Error, "Failed to deserialize engine");
        return false;
    }

    m_logger->printStdoutLog(BspLogger::LogLevel::Info, "Engine built successfully from ONNX");
    return true;
}

void tensorrt::allocateBuffers()
{
    // Allocate input buffer
    m_params.m_inputSize = getBindingSize(m_params.m_inputIndex);
    cudaMalloc(&m_params.m_deviceBuffers[m_params.m_inputIndex], m_params.m_inputSize);

    // Allocate output buffers
    m_params.m_outputSizes.resize(m_params.m_numOutputs);
    m_params.m_hostOutputBuffers.resize(m_params.m_numOutputs);

    for (int32_t i = 0; i < m_params.m_numOutputs; ++i)
    {
        int32_t bindingIdx = m_params.m_outputIndices[i];
        m_params.m_outputSizes[i] = getBindingSize(bindingIdx);

        cudaMalloc(&m_params.m_deviceBuffers[bindingIdx], m_params.m_outputSizes[i]);
        m_params.m_hostOutputBuffers[i] = malloc(m_params.m_outputSizes[i]);
    }

    m_logger->printStdoutLog(BspLogger::LogLevel::Info, "CUDA buffers allocated");
}

void tensorrt::freeBuffers()
{
    // Free device buffers
    for (int i = 0; i < 10; ++i)
    {
        if (m_params.m_deviceBuffers[i] != nullptr)
        {
            cudaFree(m_params.m_deviceBuffers[i]);
            m_params.m_deviceBuffers[i] = nullptr;
        }
    }

    // Free host buffers
    for (auto& buf : m_params.m_hostOutputBuffers)
    {
        if (buf != nullptr)
        {
            free(buf);
            buf = nullptr;
        }
    }
    m_params.m_hostOutputBuffers.clear();
}

size_t tensorrt::getBindingSize(int32_t index)
{
    const char* tensorName = nullptr;
    if (index == m_params.m_inputIndex)
    {
        tensorName = m_params.m_inputName.c_str();
    }
    else
    {
        for (int32_t i = 0; i < m_params.m_numOutputs; ++i)
        {
            if (m_params.m_outputIndices[i] == index)
            {
                tensorName = m_params.m_outputNames[i].c_str();
                break;
            }
        }
    }

    if (!tensorName)
    {
        return 0;
    }

    nvinfer1::Dims dims = m_params.m_engine->getTensorShape(tensorName);
    size_t size = 1;
    for (int32_t i = 0; i < dims.nbDims; ++i)
    {
        size *= dims.d[i];
    }

    nvinfer1::DataType dtype = m_params.m_engine->getTensorDataType(tensorName);
    switch (dtype)
    {
        case nvinfer1::DataType::kFLOAT: size *= 4; break;
        case nvinfer1::DataType::kHALF: size *= 2; break;
        case nvinfer1::DataType::kINT8: size *= 1; break;
        case nvinfer1::DataType::kINT32: size *= 4; break;
        default: break;
    }

    return size;
}

int tensorrt::getInputShape(dnnInputShape& shape)
{
    // Assuming NCHW format (batch, channel, height, width)
    if (m_params.m_inputDims.nbDims == 4)
    {
        // Standard NCHW: [batch, channel, height, width]
        shape.channel = m_params.m_inputDims.d[1];
        shape.height = m_params.m_inputDims.d[2];
        shape.width = m_params.m_inputDims.d[3];
    }
    else if (m_params.m_inputDims.nbDims == 3)
    {
        // CHW format (no batch dimension)
        shape.channel = m_params.m_inputDims.d[0];
        shape.height = m_params.m_inputDims.d[1];
        shape.width = m_params.m_inputDims.d[2];
    }
    else
    {
        m_logger->printStdoutLog(BspLogger::LogLevel::Error,
            "Unexpected input dimensions: {}", m_params.m_inputDims.nbDims);
        return -1;
    }

    m_logger->printStdoutLog(BspLogger::LogLevel::Debug,
        "Input shape: C={}, H={}, W={}", shape.channel, shape.height, shape.width);

    return 0;
}

int tensorrt::getOutputQuantParams(std::vector<int32_t>& zeroPoints, std::vector<float>& scales)
{
    // TensorRT typically uses floating point, so quantization params are not applicable
    // Fill with default values
    for (int32_t i = 0; i < m_params.m_numOutputs; ++i)
    {
        zeroPoints.push_back(0);
        scales.push_back(1.0f);
    }
    return 0;
}

int tensorrt::pushInputData(dnnInput& inputData)
{
    if (inputData.size == 0)
    {
        m_logger->printStdoutLog(BspLogger::LogLevel::Error, "Input data is empty");
        return -1;
    }

    // Copy input data to device
    cudaError_t status = cudaMemcpy(
        m_params.m_deviceBuffers[m_params.m_inputIndex],
        inputData.buf.data(),
        inputData.size,
        cudaMemcpyHostToDevice
    );

    if (status != cudaSuccess)
    {
        m_logger->printStdoutLog(BspLogger::LogLevel::Error, 
            "CUDA memcpy failed: {}", cudaGetErrorString(status));
        return -1;
    }

    // Set input tensor address (TensorRT 8.x+)
    if (!m_params.m_context->setTensorAddress(m_params.m_inputName.c_str(), 
                                               m_params.m_deviceBuffers[m_params.m_inputIndex]))
    {
        m_logger->printStdoutLog(BspLogger::LogLevel::Error, "Failed to set input tensor address");
        return -1;
    }

    return 0;
}

int tensorrt::runInference()
{
    // Set output tensor addresses (TensorRT 8.x+)
    for (int32_t i = 0; i < m_params.m_numOutputs; ++i)
    {
        int32_t bindingIdx = m_params.m_outputIndices[i];
        if (!m_params.m_context->setTensorAddress(m_params.m_outputNames[i].c_str(),
                                                   m_params.m_deviceBuffers[bindingIdx]))
        {
            m_logger->printStdoutLog(BspLogger::LogLevel::Error,
                "Failed to set output tensor address for {}", m_params.m_outputNames[i]);
            return -1;
        }
    }

    // Execute inference (TensorRT 10.x uses enqueueV3)
    cudaStream_t stream = nullptr;  // Use default CUDA stream
    bool status = m_params.m_context->enqueueV3(stream);

    if (!status)
    {
        m_logger->printStdoutLog(BspLogger::LogLevel::Error, "Inference execution failed");
        return -1;
    }

    // Synchronize to ensure completion
    cudaError_t cudaStatus = cudaStreamSynchronize(stream);
    if (cudaStatus != cudaSuccess)
    {
        m_logger->printStdoutLog(BspLogger::LogLevel::Error,
            "CUDA stream sync failed: {}", cudaGetErrorString(cudaStatus));
        return -1;
    }

    return 0;
}

int tensorrt::popOutputData(std::vector<dnnOutput>& outputVector)
{
    if (outputVector.size() != static_cast<size_t>(m_params.m_numOutputs))
    {
        outputVector.resize(m_params.m_numOutputs);
    }

    // Copy output data from device to host
    for (int32_t i = 0; i < m_params.m_numOutputs; ++i)
    {
        int32_t bindingIdx = m_params.m_outputIndices[i];

        cudaError_t status = cudaMemcpy(
            m_params.m_hostOutputBuffers[i],
            m_params.m_deviceBuffers[bindingIdx],
            m_params.m_outputSizes[i],
            cudaMemcpyDeviceToHost
        );

        if (status != cudaSuccess)
        {
            m_logger->printStdoutLog(BspLogger::LogLevel::Error,
                "CUDA memcpy D2H failed: {}", cudaGetErrorString(status));
            return -1;
        }

        outputVector[i].index = i;
        outputVector[i].buf = m_params.m_hostOutputBuffers[i];
        outputVector[i].size = m_params.m_outputSizes[i];
        outputVector[i].dataType = "float32";  // TensorRT typically outputs float32
    }

    return 0;
}

int tensorrt::releaseOutputData(std::vector<dnnOutput>& outputVector)
{
    outputVector.clear();
    return 0;
}

} // namespace bsp_dnn

