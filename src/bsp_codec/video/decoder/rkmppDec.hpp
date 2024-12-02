#ifndef RKMPPDEC_HPP
#define RKMPPDEC_HPP

#include <bsp_codec/IDecoder.hpp>

namespace bsp_codec
{

class rkmppDec : public IDecoder
{
public:
    rkmppDec() = default;
    ~rkmppDec() = default;

    // Implement necessary methods from IDecoder
    void initialize() override;
    void setDecodeReadyCallback(decodeReadyCallback callback, std::any userdata) override;
    void decode(const char* data, size_t size) override;
    void finalize() override;
};

} // namespace bsp_codec

#endif // RKMPPDEC_HPP