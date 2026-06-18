#ifndef __G2D_PREPROCESS_HELPER_HPP__
#define __G2D_PREPROCESS_HELPER_HPP__

#include <bsp_g2d/IGraphics2D.hpp>
#include <bsp_image/ImageBuffer.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace bsp_dnn
{

inline int g2dResizeToHost(bsp_g2d::IGraphics2D& g2d,
                           const bsp_perf::bsp_image::ImageView& input,
                           std::vector<uint8_t>& outputStorage,
                           uint32_t outputWidth,
                           uint32_t outputHeight,
                           const std::string& outputFormat,
                           bsp_perf::bsp_image::ImageView& output,
                           bool syncOutputToCpu = true)
{
    bsp_perf::bsp_image::ImageDesc outputDesc{};
    outputDesc.width = outputWidth;
    outputDesc.height = outputHeight;
    outputDesc.widthStride = outputWidth;
    outputDesc.heightStride = outputHeight;
    outputDesc.format = outputFormat;
    outputDesc.dataSize = bsp_perf::bsp_image::imageDataSize(outputDesc);

    outputStorage.resize(outputDesc.dataSize);
    output = bsp_perf::bsp_image::makeHostImageView(outputStorage.data(), outputDesc, outputDesc.widthStride);

    auto inputBuffer = g2d.createBuffer(bsp_g2d::IGraphics2D::BufferType::Mapped, input);
    auto outputBuffer = g2d.createBuffer(bsp_g2d::IGraphics2D::BufferType::Mapped, output);
    if (!inputBuffer || !outputBuffer) {
        g2d.releaseBuffer(inputBuffer);
        g2d.releaseBuffer(outputBuffer);
        return -1;
    }

    int ret = g2d.imageResize(inputBuffer, outputBuffer);
    if (ret == 0 && syncOutputToCpu) {
        ret = g2d.syncBuffer(outputBuffer, bsp_g2d::IGraphics2D::SyncDirection::DeviceToCpu);
    }

    g2d.releaseBuffer(outputBuffer);
    g2d.releaseBuffer(inputBuffer);
    return ret;
}

} // namespace bsp_dnn

#endif // __G2D_PREPROCESS_HELPER_HPP__
