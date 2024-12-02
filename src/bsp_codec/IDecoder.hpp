#ifndef __IDECODER_HPP__
#define __IDECODER_HPP__

#include <memory>
#include <string>
#include <any>
#include <functional>

namespace bsp_codec
{

using decodeReadyCallback = std::function<void(std::any userdata, int width_stride, int height_stride, int width, int height, int format, int fd, void* data)>;

class IDecoder {
public:
    /**
     * @brief Factory function to create an IDecoder instance.
     *
     * This static method creates and returns a unique pointer to an IDecoder
     * instance based on the specified codec platform.
     *
     * @param codecPlatform A string representing the codec platform for which
     *                      the IDecoder instance is to be created.
     * @return std::unique_ptr<IDecoder> A unique pointer to the created IDecoder instance.
     */
    static std::unique_ptr<IDecoder> create(const std::string& codecPlatform);

    virtual void initialize() = 0;
    virtual void setDecodeReadyCallback(decodeReadyCallback callback, std::any userdata) = 0;
    virtual void decode(const char* data, size_t size) = 0;
    virtual void finalize() = 0;
    virtual ~IDecoder() = default;

protected:
    IDecoder() = default;
    IDecoder(const IDecoder&) = delete;
    IDecoder& operator=(const IDecoder&) = delete;
};

} // namespace bsp_codec

#endif // __IDECODER_HPP__