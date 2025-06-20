#ifndef RKNN_YOLOV8_HPP
#define RKNN_YOLOV8_HPP

#include <bsp_dnn/IDnnObjDetectorPlugin.hpp>
#include "common/Yolov8PostProcess.hpp"

namespace bsp_dnn
{
class rknnYolov8 : public IDnnObjDetectorPlugin
{
public:
    constexpr static int RKNN_YOLOV5_OUTPUT_BATCH = 3;
    rknnYolov8() = default;
    int preProcess(ObjDetectParams& params, ObjDetectInput& inputData, IDnnEngine::dnnInput& outputData) override;
    int postProcess(const std::string& labelTextPath, const ObjDetectParams& params,
            std::vector<IDnnEngine::dnnOutput>& inputData, std::vector<ObjDetectOutputBox>& outputData)  override;
    ~rknnYolov8() = default;

private:
    Yolov8PostProcess m_yoloPostProcess;

};

} // namespace bsp_dnn

CREATE_PLUGIN_INSTANCE_YOLOV8(bsp_dnn::rknnYolov8)

#endif // RKNN_YOLOV8_HPP
