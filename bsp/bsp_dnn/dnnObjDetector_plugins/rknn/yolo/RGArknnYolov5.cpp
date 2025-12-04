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

    // 准备输出缓冲区
    size_t rknn_input_size = params.model_input_width * params.model_input_height * params.model_input_channel;
    if (m_rknn_input_buf.size() != rknn_input_size)
    {
        m_rknn_input_buf.resize(rknn_input_size);
    }

    // ========== 使用新的 IGraphics2D API ==========

    // 创建输入 YUV420 帧的 Mapped 缓冲区
    IGraphics2D::G2DBufferParams yuv420_params;
    yuv420_params.host_ptr = yuv420_frame->virt_addr;
    yuv420_params.buffer_size = yuv420_frame->valid_data_size;
    yuv420_params.width = yuv420_frame->width;
    yuv420_params.height = yuv420_frame->height;
    yuv420_params.width_stride = yuv420_frame->width_stride;
    yuv420_params.height_stride = yuv420_frame->height_stride;
    yuv420_params.format = yuv420_frame->format;

    auto yuv420_g2d_buf = m_g2d->createBuffer(IGraphics2D::BufferType::Mapped, yuv420_params);
    if (!yuv420_g2d_buf)
    {
        throw std::runtime_error("Failed to create YUV420 G2D buffer");
    }

    // 创建输出 RGB888 帧的 Mapped 缓冲区
    IGraphics2D::G2DBufferParams rgb888_params;
    rgb888_params.host_ptr = m_rknn_input_buf.data();
    rgb888_params.buffer_size = rknn_input_size;
    rgb888_params.width = params.model_input_width;
    rgb888_params.height = params.model_input_height;
    rgb888_params.width_stride = params.model_input_width;
    rgb888_params.height_stride = params.model_input_height;
    rgb888_params.format = "RGB888";

    auto rgb888_g2d_buf = m_g2d->createBuffer(IGraphics2D::BufferType::Mapped, rgb888_params);
    if (!rgb888_g2d_buf)
    {
        m_g2d->releaseBuffer(yuv420_g2d_buf);
        throw std::runtime_error("Failed to create RGB888 G2D buffer");
    }

    // 硬件加速：YUV → RGB + Resize
    // 注意：RGA 是零拷贝，直接操作 m_rknn_input_buf 内存
    int ret = m_g2d->imageResize(yuv420_g2d_buf, rgb888_g2d_buf);
    if (ret != 0)
    {
        m_g2d->releaseBuffer(rgb888_g2d_buf);
        m_g2d->releaseBuffer(yuv420_g2d_buf);
        throw std::runtime_error("imageResize failed with code: " + std::to_string(ret));
    }

    // 释放 G2D 缓冲区
    m_g2d->releaseBuffer(rgb888_g2d_buf);
    m_g2d->releaseBuffer(yuv420_g2d_buf);

    // 准备 DNN 输入数据
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

    // RGA 零拷贝：m_rknn_input_buf 已经包含转换后的数据
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