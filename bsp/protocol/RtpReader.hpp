#ifndef __RTP_READER_HPP__
#define __RTP_READER_HPP__

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <cstddef>
#include "RtpHeader.hpp"

namespace bsp_perf
{
namespace protocol
{

class RtpReader
{
public:
    RtpReader();
    virtual ~RtpReader();

    int parseHeader(const uint8_t* data, size_t size, RtpHeader& header);

    RtpPayload extractPayload(const uint8_t* pkt_data, size_t pkt_size);

    int extractPayload(const uint8_t* pkt_data, size_t pkt_size, std::vector<uint8_t>& payload);

};

} // namespace protocol
} // namespace bsp_perf


#endif // __RTP_READER_HPP__
