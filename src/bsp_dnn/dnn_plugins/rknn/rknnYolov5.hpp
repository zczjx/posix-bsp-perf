#ifndef RKNN_YOLOV5_HPP
#define RKNN_YOLOV5_HPP

#include <bsp_dnn/dnn_plugins/IDnnPlugin.hpp>

namespace bsp_dnn
{
class rknnYolov5 : public IDnnPlugin
{
public:
    rknnYolov5() = default;
    void preProcess() override;
    void postProcess() override;
    ~rknnYolov5() = default;
};

} // namespace bsp_dnn

CREATE_PLUGIN_INSTANCE(bsp_dnn::rknnYolov5)

#endif // RKNN_YOLOV5_HPP
