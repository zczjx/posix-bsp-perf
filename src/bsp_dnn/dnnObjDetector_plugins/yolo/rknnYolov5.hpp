#ifndef RKNN_YOLOV5_HPP
#define RKNN_YOLOV5_HPP

#include <bsp_dnn/IDnnObjDetectorPlugin.hpp>
#include "common/YoloPostProcess.hpp"

namespace bsp_dnn
{
class rknnYolov5 : public IDnnObjDetectorPlugin
{
public:
    constexpr static int RKNN_YOLOV5_OUTPUT_BATCH = 3;
    rknnYolov5() = default;
    int preProcess(ObjDetectParams& params, ObjDetectInput& inputData, IDnnEngine::dnnInput& outputData) override;
    int postProcess(const std::string& labelTextPath, const ObjDetectParams& params,
            std::vector<IDnnEngine::dnnOutput>& inputData, std::vector<ObjDetectOutputBox>& outputData)  override;
    ~rknnYolov5() = default;

private:
    YoloPostProcess m_yoloPostProcess;

};

} // namespace bsp_dnn

CREATE_PLUGIN_INSTANCE(bsp_dnn::rknnYolov5)

#endif // RKNN_YOLOV5_HPP
