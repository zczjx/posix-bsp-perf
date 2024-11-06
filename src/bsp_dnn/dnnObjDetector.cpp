#include "dnnObjDetector.hpp"
#include "IDnnObjDetectorPlugin.hpp"
#include <dlfcn.h>

namespace bsp_dnn
{

dnnObjDetector::dnnObjDetector(const std::string& dnnType, const std::string& pluginPath, const std::string& labelTextPath):
    m_logger{std::make_unique<BspLogger>("dnnObjDetector")},
    m_dnnEngine{IDnnEngine::create(dnnType)},
    m_labelTextPath{labelTextPath}
{
    if (pluginPath.empty())
    {
        m_logger->printStdoutLog(BspLogger::LogLevel::Error, "pluginPath is empty.");
        return;
    }

    m_pluginLibraryHandle = std::shared_ptr<void>(dlopen(pluginPath.c_str(), RTLD_LAZY), dlclose);

    if (m_pluginLibraryHandle == nullptr)
    {
        m_logger->printStdoutLog(BspLogger::LogLevel::Error, "Failed to open plugin library: {}", dlerror());
        throw std::runtime_error(dlerror());
    }

    auto create = reinterpret_cast<IDnnObjDetectorPlugin* (*)()>(dlsym(m_pluginLibraryHandle.get(), "create"));
    if (create == nullptr)
    {
        m_logger->printStdoutLog(BspLogger::LogLevel::Error, "Failed to load symbol create: {}", dlerror());
        throw std::runtime_error(dlerror());
    }

    m_dnnPluginHandle.reset(create());

    if (m_dnnPluginHandle == nullptr)
    {
        m_logger->printStdoutLog(BspLogger::LogLevel::Error, "Failed to create plugin instance.");
        throw std::runtime_error("Failed to create plugin instance.");
    }

}

dnnObjDetector::~dnnObjDetector()
{
    if (m_dnnPluginHandle != nullptr)
    {
        auto destroy = reinterpret_cast<void (*)(IDnnObjDetectorPlugin*)>(dlsym(m_pluginLibraryHandle.get(), "destroy"));
        if (destroy == nullptr)
        {
            m_logger->printStdoutLog(BspLogger::LogLevel::Error, "Failed to load symbol destroy: {}", dlerror());
            throw std::runtime_error(dlerror());
        }

        destroy(m_dnnPluginHandle.get());
    }

    if (m_pluginLibraryHandle != nullptr)
    {
        dlclose(m_pluginLibraryHandle.get());
    }
}

void dnnObjDetector::loadModel(const std::string& modelPath)
{
    m_dnnEngine->loadModel(modelPath);
}

void dnnObjDetector::pushInputData(std::shared_ptr<ObjDetectInput> dataInput)
{
    m_dataInput = dataInput;
}

std::vector<ObjDetectOutputBox>& dnnObjDetector::popOutputData()
{
    return m_dataOutputVector;
}

int dnnObjDetector::defaultPreProcess(ObjDetectInput& inputData, IDnnEngine::dnnInput& outputData)
{
    return 0;
}

int dnnObjDetector::defaultPostProcess(const std::string& labelTextPath, const ObjDetectParams& params,
        std::vector<IDnnEngine::dnnOutput>& inputData, std::vector<ObjDetectOutputBox>& outputData)
{
    return 0;
}

int dnnObjDetector::runObjDetect(const ObjDetectParams& params)
{
    if ((m_pluginLibraryHandle == nullptr) || (m_dnnPluginHandle == nullptr))
    {
        m_logger->printStdoutLog(BspLogger::LogLevel::Error, "pluginLibraryHandle is nullptr.");
        IDnnEngine::dnnInput dnn_input_tensor{};
        defaultPreProcess(*m_dataInput, dnn_input_tensor);
        m_dnnEngine->pushInputData(dnn_input_tensor);
        m_dnnEngine->runInference();
        std::vector<IDnnEngine::dnnOutput> dnn_output_vector{};
        m_dnnEngine->popOutputData(dnn_output_vector);
        return defaultPostProcess(m_labelTextPath, params,
                    dnn_output_vector, m_dataOutputVector);
    }

    IDnnEngine::dnnInput dnn_input_tensor{};
    m_dnnPluginHandle->preProcess(*m_dataInput, dnn_input_tensor);
    m_dnnEngine->pushInputData(dnn_input_tensor);
    m_dnnEngine->runInference();
    std::vector<IDnnEngine::dnnOutput> dnn_output_vector{};
    m_dnnEngine->popOutputData(dnn_output_vector);
    return m_dnnPluginHandle->postProcess(m_labelTextPath, params,
                dnn_output_vector, m_dataOutputVector);
}


} // namespace bsp_dnn