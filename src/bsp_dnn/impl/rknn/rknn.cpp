#include "rknn.hpp"
#include <bsp_dnn/dnn_plugins/IDnnPlugin.hpp>
#include <dlfcn.h>

namespace bsp_dnn
{
template <typename T>
rknn<T>::rknn(const std::string& pluginPath):
    m_logger{std::make_unique<BspLogger>("rknn")}
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

    auto create = reinterpret_cast<IDnnPlugin* (*)()>(dlsym(m_pluginLibraryHandle.get(), "create"));
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

template <typename T>
rknn<T>::~rknn()
{
    if (m_dnnPluginHandle != nullptr)
    {
        auto destroy = reinterpret_cast<void (*)(IDnnPlugin*)>(dlsym(m_pluginLibraryHandle.get(), "destroy"));
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

} // namespace bsp_dnn