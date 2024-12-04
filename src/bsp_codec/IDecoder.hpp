#ifndef __IDECODER_HPP__
#define __IDECODER_HPP__

#include <memory>
#include <string>
#include <any>
#include <functional>

namespace bsp_codec
{

using decodeReadyCallback = std::function<void(std::any userdata, int width_stride, int height_stride, int width, int height, int format, int fd, std::any data)>;

struct DecodeConfig
{
    // encoding can be "h264", "h265"
    std::string encoding{"h264"};
    int fps{0};
    std::any data{nullptr};
};

struct DecodePacket
{
    uint8_t* data{nullptr};
    size_t pkt_size{0};
    int pkt_eos;
};

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