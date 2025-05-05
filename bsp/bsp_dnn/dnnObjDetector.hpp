#ifndef __DNN_OBJDETECTOR_HPP__
#define __DNN_OBJDETECTOR_HPP__

#include "IDnnObjDetectorPlugin.hpp"
#include "IDnnEngine.hpp"
#include <shared/BspLogger.hpp>
#include <memory>
#include <string>
#include <vector>

namespace bsp_dnn
{
using namespace bsp_perf::shared;

class dnnObjDetector
{
public:

    /**
     * @brief Constructor for the dnn Detector class.
     *
     * This function creates and returns a unique pointer to an instance of dnn Detector,
     *
     * @param dnnType A string specifying the type of DNN to create.
     * - "trt" for TensorRT DNN
     * - "rknn" for RKNN DNN
     * @param pluginPath A string specifying the path to the plugin library.
     *
     * @param labelTextPath A string specifying the path to the label text file.
     *
     * @return dnnObjDetector A unique pointer to the created dnn Detector instance.
     */
    dnnObjDetector(const std::string& dnnType, const std::string& pluginPath, const std::string& pluginType, const std::string& labelTextPath);

    virtual ~dnnObjDetector();

    // Pure virtual function for loading a model
    void loadModel(const std::string& modelPath);

    int getInputShape(IDnnEngine::dnnInputShape& shape)
    {
        return m_dnnEngine->getInputShape(shape);
    }
    
    int getOutputAttr(std::vector<rknn_tensor_attr>& output_attrs)
    {
        return m_dnnEngine->getOutputAttr(output_attrs);
    }

    int getInputOutputNum(rknn_input_output_num& io_num)
    {
        return m_dnnEngine->getInputOutputNum(io_num);
    }

    int getOutputQuantParams(std::vector<int32_t>& zeroPoints, std::vector<float>& scales)
    {
        return m_dnnEngine->getOutputQuantParams(zeroPoints, scales);
    }

    void pushInputData(std::shared_ptr<ObjDetectInput> dataInput);

    std::vector<ObjDetectOutputBox>& popOutputData();

    // Pure virtual function for running inference
    int runObjDetect(ObjDetectParams& params);

private:

    int defaultPreProcess(ObjDetectInput& inputData, IDnnEngine::dnnInput& outputData);

    int defaultPostProcess(const std::string& labelTextPath, const ObjDetectParams& params,
            std::vector<IDnnEngine::dnnOutput>& inputData, std::vector<ObjDetectOutputBox>& outputData);

private:
    std::shared_ptr<void> m_pluginLibraryHandle{nullptr};
    std::shared_ptr<IDnnObjDetectorPlugin> m_dnnPluginHandle{nullptr};
    std::unique_ptr<IDnnEngine> m_dnnEngine{nullptr};
    std::unique_ptr<BspLogger> m_logger{nullptr};
    std::shared_ptr<ObjDetectInput> m_dataInput{nullptr};
    std::vector<ObjDetectOutputBox> m_dataOutputVector;
    std::string m_labelTextPath;
};

} // namespace bsp_dnn

#endif // __DNN_OBJDETECTOR_HPP__