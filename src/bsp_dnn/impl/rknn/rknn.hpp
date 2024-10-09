#ifndef RKNN_HPP
#define RKNN_HPP

#include <bsp_dnn/IDnn.hpp>

namespace bsp_dnn {

template<typename T>
class rknn : public IDnn<T>
{

public:
    explicit rknn(const std::string& pluginPath="");
    rknn(const rknn&) = delete;
    rknn& operator=(const rknn&) = delete;
    rknn(rknn&&) = delete;
    rknn& operator=(rknn&&) = delete;
    ~rknn();

    // Pure virtual function for loading a model
    virtual void loadModel(const std::string& modelPath);

    virtual void pushInputData(const std::vector<T>& inputData);

    virtual void popOutputData(std::vector<T>& outputData);

    // Pure virtual function for running inference
    virtual void runInference(const std::vector<T>& inputData, std::vector<T>& outputData);

protected:

    virtual void runPreProcess(const std::vector<T>& inputData, std::vector<T>& outputData);

    virtual void runPostProcess(const std::vector<T>& inputData, std::vector<T>& outputData);

private:
    std::shared_ptr<void> m_pluginLibraryHandle{nullptr};
    std::shared_ptr<IDnnPlugin> m_dnnPluginHandle{nullptr};
    std::unique_ptr<BspLogger> m_logger;
};
} // namespace bsp_dnn

#endif // RKNN_HPP