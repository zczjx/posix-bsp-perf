#ifndef __IDECODER_HPP__
#define __IDECODER_HPP__

#include <memory>
#include <string>
#include <any>
#include <functional>
#include <image/ImageTypes.hpp>

namespace bsp_codec
{
/**
 * @brief DecodeConfig for the decoder.
 *
 * @var DecodeConfig::encoding
 * Encoding type of the input stream. Supported values:
 * - "h264"
 * - "h265"
 * - "vp8"
 * - "vp9"
 * - "mpeg2"
 * - "mpeg4"
 */
struct DecodeConfig
{
    // encoding can be "h264", "h265"
    std::string encoding{"h264"};
    int fps{0};
};

struct DecodePacket
{
    uint8_t* data{nullptr};
    size_t pkt_size{0};
    int pkt_eos;
};


/**
 * @struct DecodeOutFrame
 * @brief Structure representing the output frame of the decoder.
 *
 * @var DecodeOutFrame::width
 * Width of the output frame.
 *
 * @var DecodeOutFrame::height
 * Height of the output frame.
 *
 * @var DecodeOutFrame::width_stride
 * Stride of the width of the output frame.
 *
 * @var DecodeOutFrame::height_stride
 * Stride of the height of the output frame.
 *
 * @var DecodeOutFrame::format
 * Format of the output frame. Supported values:
 * - "YUV420SP"
 *
 *
 * @var DecodeOutFrame::virt_addr
 * Virtual address of the output frame.
 *
 * @var DecodeOutFrame::fd
 * File descriptor associated with the output frame.
 */
struct DecodeOutFrame
{
    int width;
    int height;
    int width_stride;
    int height_stride;
    std::string format{"YUV420SP"};
    uint8_t *virt_addr{nullptr};
    size_t valid_data_size{0};
    int fd;
    int eos_flag;
};

inline bsp_perf::image::ImageView toImageView(const DecodeOutFrame& frame)
{
    bsp_perf::image::ImageView view;
    view.desc.width = static_cast<uint32_t>(frame.width);
    view.desc.height = static_cast<uint32_t>(frame.height);
    view.desc.widthStride = static_cast<uint32_t>(frame.width_stride > 0 ? frame.width_stride : frame.width);
    view.desc.heightStride = static_cast<uint32_t>(frame.height_stride > 0 ? frame.height_stride : frame.height);
    view.desc.format = frame.format;
    view.desc.dataSize = frame.valid_data_size;
    view.memoryType = frame.fd >= 0 ? bsp_perf::image::ImageMemoryType::DmaBuf : bsp_perf::image::ImageMemoryType::Host;
    view.planeCount = 1;
    view.planes[0].data = frame.virt_addr;
    view.planes[0].size = frame.valid_data_size;
    view.planes[0].rowStride = view.desc.widthStride;
    view.planes[0].fd = frame.fd;
    return view;
}

inline bsp_perf::image::ImageView toImageView(const std::shared_ptr<DecodeOutFrame>& frame)
{
    if (!frame) {
        return {};
    }

    auto view = toImageView(*frame);
    view.owner = frame;
    return view;
}

inline DecodeOutFrame fromImageView(const bsp_perf::image::ImageView& view)
{
    DecodeOutFrame frame{};
    frame.width = static_cast<int>(view.desc.width);
    frame.height = static_cast<int>(view.desc.height);
    frame.width_stride = static_cast<int>(view.desc.widthStride);
    frame.height_stride = static_cast<int>(view.desc.heightStride);
    frame.format = view.desc.format;
    frame.virt_addr = view.planes[0].data;
    frame.valid_data_size = view.desc.dataSize;
    frame.fd = view.planes[0].fd;
    frame.eos_flag = 0;
    return frame;
}

using decodeReadyCallback = std::function<void(std::any userdata, std::shared_ptr<DecodeOutFrame> frame)>;

class IDecoder
{
public:
    /**
     * @brief Factory function to create an IDecoder instance.
     *
     * This static method creates and returns a unique pointer to an IDecoder
     * instance based on the specified codec platform.
     *
     * @param codecPlatform A string representing the codec platform for which
     * the IDecoder instance is to be created.
     * Supported values:
     * - "rkmpp" for Rockchip MPP decoder
     * - "nvdec" for NVIDIA NVDEC decoder (Jetson platforms)
     *
     * @throws std::invalid_argument If an invalid codec platform is specified.
     * @return std::unique_ptr<IDecoder> A unique pointer to the created IDecoder instance.
     */
    static std::unique_ptr<IDecoder> create(const std::string& codecPlatform);

    virtual int setup(DecodeConfig& cfg) = 0;
    virtual void setDecodeReadyCallback(decodeReadyCallback callback, std::any userdata) = 0;
    virtual int decode(DecodePacket& pkt_data) = 0;
    virtual int tearDown() = 0;
    virtual ~IDecoder() = default;

protected:
    IDecoder() = default;
    IDecoder(const IDecoder&) = delete;
    IDecoder& operator=(const IDecoder&) = delete;
};

} // namespace bsp_codec

#endif // __IDECODER_HPP__