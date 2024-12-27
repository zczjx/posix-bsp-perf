#ifndef __RGA_RKNN_YOLOV5_HPP__
#define __RGA_RKNN_YOLOV5_HPP__

#include <bsp_dnn/IDnnObjDetectorPlugin.hpp>
#include <bsp_g2d/IGraphics2D.hpp>
#include "common/YoloPostProcess.hpp"
#include <memory>


namespace bsp_dnn
{
using namespace bsp_g2d;
class RGArknnYolov5 : public IDnnObjDetectorPlugin
{
public:
    constexpr static int RKNN_YOLOV5_OUTPUT_BATCH = 3;
    RGArknnYolov5():
        m_g2d{IGraphics2D::create("rkrga")},
        IDnnObjDetectorPlugin()
    {}
    int preProcess(ObjDetectParams& params, ObjDetectInput& inputData, IDnnEngine::dnnInput& outputData) override;
    int postProcess(const std::string& labelTextPath, const ObjDetectParams& params,
            std::vector<IDnnEngine::dnnOutput>& inputData, std::vector<ObjDetectOutputBox>& outputData)  override;
    ~RGArknnYolov5() = default;

private:
    YoloPostProcess m_yoloPostProcess{};
    std::unique_ptr<IGraphics2D> m_g2d{nullptr};
    std::shared_ptr<IGraphics2D::G2DBuffer> m_g2dBuffer{nullptr};
    std::vector<uint8_t> m_rknn_input_buf{};

};

} // namespace bsp_dnn

CREATE_PLUGIN_INSTANCE(bsp_dnn::RGArknnYolov5)

#endif // __RGA_RKNN_YOLOV5_HPP__
