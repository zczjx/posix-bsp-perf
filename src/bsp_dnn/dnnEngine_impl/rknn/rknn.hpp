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
    rknn_context m_rknnCtx{nullptr};
    int32_t m_model_size;
    rknn_sdk_version m_version;
    rknn_input_output_num m_io_num;
    std::vector<rknn_tensor_attr> m_input_attrs{};
    std::vector<rknn_tensor_attr> m_output_attrs{};
    rknn_input m_inputs[1];
    std::vector<rknn_output> m_outputs{};
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