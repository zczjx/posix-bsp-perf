#include "RGArknnYolov5.hpp"
#include <bsp_codec/IDecoder.hpp>
#include <memory>
#include <any>
#include <iostream>
#include <cstring>

namespace bsp_dnn
{
using namespace bsp_codec;

int RGArknnYolov5::preProcess(ObjDetectParams& params, ObjDetectInput& inputData, IDnnEngine::dnnInput& outputData)
{
    if (inputData.handleType.compare("DecodeOutFrame") != 0)
    {
        throw std::invalid_argument("Only DecodeOutFrame is supported.");
    }

    auto yuv420_frame = std::any_cast<std::shared_ptr<DecodeOutFrame>>(inputData.imageHandle);
    if (yuv420_frame == nullptr)
    {
        throw std::invalid_argument("inputData.imageHandle is nullptr.");
    }

    if (m_g2d == nullptr)
    {
        throw std::invalid_argument("m_g2d is nullptr.");
    }

    size_t rknn_input_size = params.model_input_width * params.model_input_height * params.model_input_channel;
    if (m_rknn_input_buf.size() != rknn_input_size)
    {
        m_rknn_input_buf.resize(rknn_input_size);
    }

    IGraphics2D::G2DBufferParams yuv420_frame_g2d_params = {
        .virtual_addr = yuv420_frame->virt_addr,
        .width = yuv420_frame->width,
        .height = yuv420_frame->height,
        .width_stride = yuv420_frame->width_stride,
        .height_stride = yuv420_frame->height_stride,
        .format = yuv420_frame->format,
    };
    std::shared_ptr<IGraphics2D::G2DBuffer> yuv420_frame_g2d_buf = m_g2d->createG2DBuffer("virtualaddr", yuv420_frame_g2d_params);

    IGraphics2D::G2DBufferParams rgb888_frame_g2d_params = {
        .virtual_addr = m_rknn_input_buf.data(),
        .width = params.model_input_width,
        .height = params.model_input_height,
        .width_stride = params.model_input_width,
        .height_stride = params.model_input_height,
        .format = "RGB888",
    };
    std::shared_ptr<IGraphics2D::G2DBuffer> rgb888_frame_g2d_buf = m_g2d->createG2DBuffer("virtualaddr", rgb888_frame_g2d_params);

    m_g2d->imageResize(yuv420_frame_g2d_buf, rgb888_frame_g2d_buf);

    outputData.index = 0;
    outputData.shape.width = params.model_input_width;
    outputData.shape.height = params.model_input_height;
    outputData.shape.channel = params.model_input_channel;
    outputData.size = rknn_input_size;
    if (outputData.buf.size() != outputData.size)
    {
        outputData.buf.resize(outputData.size);
    }
    outputData.dataType = "UINT8";
    std::memcpy(outputData.buf.data(), m_rknn_input_buf.data(), outputData.size);
    return 0;
}

int RGArknnYolov5::postProcess(const std::string& labelTextPath, const ObjDetectParams& params,
        std::vector<IDnnEngine::dnnOutput>& inputData, std::vector<ObjDetectOutputBox>& outputData)
{
    if (inputData.size() != RKNN_YOLOV5_OUTPUT_BATCH || labelTextPath.empty())
    {
        throw std::invalid_argument("The size of inputData is not equal to RKNN_YOLOV5_OUTPUT_BATCH or labelTextPath is empty.");
    }

    int ret = m_yoloPostProcess.initLabelMap(labelTextPath);
    if (ret != 0)
    {
        return ret;
    }
    return m_yoloPostProcess.runPostProcess(params, inputData, outputData);
}

}   // namespace bsp_dnn