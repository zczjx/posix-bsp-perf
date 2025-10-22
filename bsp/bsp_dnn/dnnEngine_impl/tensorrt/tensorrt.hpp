#ifndef __TENSORRT_HPP__
#define __TENSORRT_HPP__

#include <bsp_dnn/IDnnEngine.hpp>
#include <shared/BspLogger.hpp>
#include <NvInfer.h>
#include <NvOnnxParser.h>
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <fstream>


namespace bsp_dnn
{

using namespace bsp_perf::shared;

// TensorRT Logger
class TrtLogger : public nvinfer1::ILogger
{
public:
    void log(Severity severity, const char* msg) noexcept override
    {
        if (severity <= Severity::kWARNING)
        {
            std::cout << "[TensorRT] " << msg << std::endl;
        }
    }
};

struct TensorRTParams
{
    // TensorRT runtime objects
    nvinfer1::IRuntime* m_runtime{nullptr};
    nvinfer1::ICudaEngine* m_engine{nullptr};
    nvinfer1::IExecutionContext* m_context{nullptr};

    // CUDA buffers
    void* m_deviceBuffers[10]{nullptr};  // Support up to 10 I/O tensors

    // Input/Output info
    int32_t m_inputIndex{-1};
    int32_t m_outputIndices[8]{-1};  // Support multiple outputs
    int32_t m_numInputs{0};
    int32_t m_numOutputs{0};

    // Tensor names (TensorRT 8.x+ API)
    std::string m_inputName{};
    std::vector<std::string> m_outputNames{};

    // Input dimensions (NCHW format)
    nvinfer1::Dims m_inputDims;
    std::vector<nvinfer1::Dims> m_outputDims{};

    // Data sizes
    size_t m_inputSize{0};
    std::vector<size_t> m_outputSizes{};

    // Host buffers for outputs
    std::vector<void*> m_hostOutputBuffers{};
};

class tensorrt : public IDnnEngine
{
public:
    explicit tensorrt();
    tensorrt(const tensorrt&) = delete;
    tensorrt& operator=(const tensorrt&) = delete;
    tensorrt(tensorrt&&) = delete;
    tensorrt& operator=(tensorrt&&) = delete;
    ~tensorrt();

    // IDnnEngine interface implementation
    void loadModel(const std::string& modelPath) override;
    int getInputShape(dnnInputShape& shape) override;
    int getOutputQuantParams(std::vector<int32_t>& zeroPoints, std::vector<float>& scales) override;
    int pushInputData(dnnInput& inputData) override;
    int popOutputData(std::vector<dnnOutput>& outputVector) override;
    int runInference() override;
    int releaseOutputData(std::vector<dnnOutput>& outputVector) override;

private:
    // Helper methods
    bool loadEngineFromFile(const std::string& enginePath);
    bool buildEngineFromOnnx(const std::string& onnxPath);
    void allocateBuffers();
    void freeBuffers();
    size_t getBindingSize(int32_t index);

private:
    TrtLogger m_trtLogger{};
    TensorRTParams m_params{};
    std::unique_ptr<BspLogger> m_logger;
};

} // namespace bsp_dnn

#endif // __TENSORRT_HPP__

