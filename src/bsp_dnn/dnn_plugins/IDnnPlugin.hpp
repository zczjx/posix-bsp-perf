// IPlugin.h
#ifndef IPLUGIN_H
#define IPLUGIN_H

class IPlugin {
public:
    virtual void execute() = 0; // 纯虚函数
    virtual ~IPlugin() {}
};

#define CREATE_PLUGIN_INSTANCE() \
    extern "C" IPlugin* create() { \
        return new MyPlugin(); \
    } \
    extern "C" void destroy(IPlugin* plugin) { \
        delete plugin; \
    }

#endif // IPLUGIN_H