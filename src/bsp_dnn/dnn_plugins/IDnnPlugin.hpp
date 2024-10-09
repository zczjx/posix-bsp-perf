#ifndef IDNN_PLUGIN_HPP
#define IDNN_PLUGIN_HPP

namespace bsp_dnn
{

class IDnnPlugin
{
public:
    virtual void preProcess() = 0; // 纯虚函数
    virtual void postProcess() = 0; // 纯虚函数
    virtual ~IDnnPlugin() = default;
};

}

#define CREATE_PLUGIN_INSTANCE(PLUGIN_CLASS) \
    extern "C" bsp_dnn::IDnnPlugin* create() { \
        return new PLUGIN_CLASS(); \
    } \
    extern "C" void destroy(bsp_dnn::IDnnPlugin* plugin) { \
        delete plugin; \
    }

#endif // IDNN_PLUGIN_HPP