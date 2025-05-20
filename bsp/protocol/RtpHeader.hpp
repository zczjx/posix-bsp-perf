#ifndef __RTP_HEADER_HPP__
#define __RTP_HEADER_HPP__

#include <cstdint>
#include <cstddef>
namespace bsp_perf
{
namespace protocol
{

struct RtpHeader
{
    uint8_t start_of_header;
    uint8_t payload_type;
    uint16_t sequence_number;
    uint32_t timestamp;
    uint32_t ssrc;
};

struct RtpPayload
{
    uint8_t* data;
    size_t size;
};

} // namespace protocol
} // namespace bsp_perf

#endif // __RTP_HEADER_HPP__
