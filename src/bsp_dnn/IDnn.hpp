#ifndef IDNN_HPP
#define IDNN_HPP

#include <memory>
#include <string>
#include <vector>

namespace bsp_dnn {

template<typename T>
class IDnn
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
    static std::unique_ptr<IDnn<T>> create(const std::string& dnnType, const std::string& pluginPath);


    virtual ~IDnn() = default;

    // Pure virtual function for loading a model
    virtual void loadModel(const std::string& modelPath) = 0;

    virtual void pushInputData(const std::vector<T>& inputData) = 0;

    virtual void popOutputData(std::vector<T>& outputData) = 0;

    // Pure virtual function for running inference
    virtual void runInference(const std::vector<T>& inputData, std::vector<T>& outputData) = 0;

protected:

    virtual void runPreProcess(const std::vector<T>& inputData, std::vector<T>& outputData) = 0;

    virtual void runPostProcess(const std::vector<T>& inputData, std::vector<T>& outputData) = 0;

};

} // namespace bsp_dnn

#endif // IDNN_HPP