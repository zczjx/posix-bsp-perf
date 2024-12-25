#ifndef __IDECODER_HPP__
#define __IDECODER_HPP__

#include <memory>
#include <string>
#include <any>
#include <functional>

namespace bsp_codec
{
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
 * - "RGBA_8888"
 * - "RGBX_8888"
 * - "RGB_888"
 * - "BGRA_8888"
 * - "RGB_565"
 * - "RGBA_5551"
 * - "RGBA_4444"
 * - "BGR_888"
 * - "YCbCr_422_SP"
 * - "YCbCr_422_P"
 * - "YCbCr_420_SP"
 * - "YCbCr_420_P"
 * - "YCrCb_422_SP"
 * - "YCrCb_422_P"
 * - "YCrCb_420_SP"
 * - "YCrCb_420_P"
 * - "BPP1"
 * - "BPP2"
 * - "BPP4"
 * - "BPP8"
 * - "Y4"
 * - "YCbCr_400"
 * - "BGRX_8888"
 * - "YVYU_422"
 * - "YVYU_420"
 * - "VYUY_422"
 * - "VYUY_420"
 * - "YUYV_422"
 * - "YUYV_420"
 * - "UYVY_422"
 * - "UYVY_420"
 * - "YCbCr_420_SP_10B"
 * - "YCrCb_420_SP_10B"
 * - "YCbCr_422_SP_10B"
 * - "YCrCb_422_SP_10B"
 * - "BGR_565"
 * - "BGRA_5551"
 * - "BGRA_4444"
 * - "ARGB_8888"
 * - "XRGB_8888"
 * - "ARGB_5551"
 * - "ARGB_4444"
 * - "ABGR_8888"
 * - "XBGR_8888"
 * - "ABGR_5551"
 * - "ABGR_4444"
 * - "RGBA2BPP"
 * - "A8"
 * - "YCbCr_444_SP"
 * - "YCrCb_444_SP"
 * - "Y8"
 * - "UNKNOWN"
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
    char *virt_addr{nullptr};
    int fd;
    int eos_flag;
};

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
     *
     * @throws std::invalid_argument If an invalid codec platform is specified.
     * @return std::unique_ptr<IDecoder> A unique pointer to the created IDecoder instance.
     */
    static std::unique_ptr<IDecoder> create(const std::string& codecPlatform);

    virtual int setup(DecodeConfig& cfg) = 0;
    virtual void setDecodeReadyCallback(decodeReadyCallback callback, std::any userdata) = 0;
    virtual int decode(DecodePacket& pkt_data) = 0;
    virtual int reset() = 0;
    virtual ~IDecoder() = default;

protected:
    IDecoder() = default;
    IDecoder(const IDecoder&) = delete;
    IDecoder& operator=(const IDecoder&) = delete;
};

} // namespace bsp_codec

#endif // __IDECODER_HPP__