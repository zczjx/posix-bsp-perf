
#ifndef __IDNN_ENGINE_HPP__
#define __IDNN_ENGINE_HPP__

#include <memory>
#include <string>
#include <vector>

namespace bsp_dnn {

class IDnnEngine
{
public:

    /**
     * @brief Static factory function to create instances of derived classes.
     *
     * This function creates and returns a unique pointer to an instance of a class derived from IDnn,
     * based on the specified DNN type.
     *
     * @param dnnType A string specifying the type of DNN to create.
     * - "trt" for TensorRT DNN
     * - "rknn" for RKNN DNN
     * @return std::unique_ptr<IDnn> A unique pointer to the created DNN instance.
     */
    static std::unique_ptr<IDnnEngine> create(const std::string& dnnType);

    struct dnnInputShape
    {
        size_t width{0};
        size_t height{0};
        size_t channel{0};
    };

    struct dnnInput
    {
        size_t index{0};
        std::shared_ptr<uint8_t> buf{nullptr};
        size_t size{0};
        dnnInputShape shape{};
        // dataType can be "UINT8", "float32"
        std::string dataType{"UINT8"};
    };

    struct dnnOutput
    {
        size_t index{0};
        void* buf{nullptr};
        size_t size{0};
        // dataType can be "UINT8", "INT8", "float32"
        std::string dataType{"INT8"};
    };

    virtual ~IDnnEngine() = default;

    // Pure virtual function for loading a model
    virtual void loadModel(const std::string& modelPath) = 0;

    virtual int getInputShape(dnnInputShape& shape) = 0;

    virtual int getOutputQuantParams(std::vector<int32_t>& zeroPoints, std::vector<float>& scales) = 0;

    virtual int pushInputData(dnnInput& inputData) = 0;

    virtual int popOutputData(std::vector<dnnOutput>& outputVector) = 0;

    // Pure virtual function for running inference
    virtual int runInference() = 0;


};

} // namespace bsp_dnn

#endif // __IDNN_ENGINE_HPP__