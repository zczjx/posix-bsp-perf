#include "RtpReader.hpp"

namespace bsp_perf
{
namespace protocol
{

RtpReader::RtpReader()
{
}

RtpReader::~RtpReader()
{
}

int RtpReader::parseHeader(const uint8_t* data, size_t size, RtpHeader& header)
{
    if (size < sizeof(RtpHeader))
    {
        return -1;
    }

    header.start_of_header = data[0];
    header.payload_type = data[1];
    header.sequence_number = (data[2] << 8) | data[3];
    header.timestamp = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
    header.ssrc = (data[8] << 24) | (data[9] << 16) | (data[10] << 8) | data[11];
    return 0;
}

RtpPayload RtpReader::extractPayload(const uint8_t* pkt_data, size_t pkt_size)
{
    RtpPayload payload{nullptr, 0};

    if (pkt_size < sizeof(RtpHeader))
    {
        return payload;
    }

    payload.data = const_cast<uint8_t*>(pkt_data + sizeof(RtpHeader));
    payload.size = pkt_size - sizeof(RtpHeader);
    return payload;
}

int RtpReader::extractPayload(const uint8_t* pkt_data, size_t pkt_size, std::vector<uint8_t>& payload)
{
    if (pkt_size < sizeof(RtpHeader))
    {
        return -1;
    }

    payload.assign(pkt_data + sizeof(RtpHeader), pkt_data + pkt_size);
    return 0;
}

} // namespace protocol
} // namespace bsp_perf