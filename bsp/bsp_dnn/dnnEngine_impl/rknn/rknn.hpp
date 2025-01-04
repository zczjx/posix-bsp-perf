#ifndef __RKNN_HPP__
#define __RKNN_HPP__

#include <bsp_dnn/IDnnEngine.hpp>
#include <rockchip/rknn_api.h>
#include <shared/BspLogger.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <memory>


namespace bsp_dnn {

using namespace bsp_perf::shared;

struct RknnParams
{
    std::shared_ptr<unsigned char> m_model_data{nullptr};
    rknn_context m_rknnCtx;
    int32_t m_model_size;
    rknn_sdk_version m_version;
    rknn_input_output_num m_io_num;
    std::vector<rknn_tensor_attr> m_input_attrs{};
    std::vector<rknn_tensor_attr> m_output_attrs{};
    rknn_input m_inputs[1];
    std::vector<rknn_output> m_outputs{};
    const std::unordered_map<std::string, rknn_tensor_type> dataTypeMap{
        {"FP32", rknn_tensor_type::RKNN_TENSOR_FLOAT32},
        {"FP16", rknn_tensor_type::RKNN_TENSOR_FLOAT16},
        {"INT8", rknn_tensor_type::RKNN_TENSOR_INT8},
        {"UINT8", rknn_tensor_type::RKNN_TENSOR_UINT8},
        {"INT16", rknn_tensor_type::RKNN_TENSOR_INT16},
        {"UINT16", rknn_tensor_type::RKNN_TENSOR_UINT16},
        {"INT32", rknn_tensor_type::RKNN_TENSOR_INT32},
        {"UINT32", rknn_tensor_type::RKNN_TENSOR_UINT32},
        {"INT64", rknn_tensor_type::RKNN_TENSOR_INT64},
        {"BOOL", rknn_tensor_type::RKNN_TENSOR_BOOL},
        {"INT4", rknn_tensor_type::RKNN_TENSOR_INT4},
        {"BFLOAT16", rknn_tensor_type::RKNN_TENSOR_BFLOAT16}

    };
};

class rknn : public IDnnEngine
{

public:
    explicit rknn();
    rknn(const rknn&) = delete;
    rknn& operator=(const rknn&) = delete;
    rknn(rknn&&) = delete;
    rknn& operator=(rknn&&) = delete;
    ~rknn();

    // Pure virtual function for loading a model
    void loadModel(const std::string& modelPath) override;

    int getInputShape(dnnInputShape& shape) override;

    int getOutputQuantParams(std::vector<int32_t>& zeroPoints, std::vector<float>& scales) override;

    int pushInputData(dnnInput& inputData) override;

    int popOutputData(std::vector<dnnOutput>& outputVector)  override;

    // Pure virtual function for running inference
    int runInference() override;


private:

    std::shared_ptr<unsigned char> loadModelFile(const std::string& modelPath);

    std::shared_ptr<unsigned char> loadModelData(FILE* fp, size_t offset, size_t sz);

private:

    RknnParams m_params{};
    std::unique_ptr<BspLogger> m_logger;
};
} // namespace bsp_dnn

#endif // RKNN_HPP