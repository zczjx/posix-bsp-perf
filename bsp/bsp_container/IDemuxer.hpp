#ifndef __IDEMUXER_HPP__
#define __IDEMUXER_HPP__

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace bsp_container
{

struct StreamInfo
{
    std::string codec{};
    std::string codecProfile{};
    std::string codecDescription{};
    int width{0};
    int height{0};
    int frame_rate_num{0};
    int frame_rate_den{0};
    int bit_rate{0};
    int numChannels{0};
    int sample_rate{0};
    int bits_per_sample{0};
};
struct ContainerInfo
{
    std::string containerType{};
    size_t numStreams{0};
    std::unordered_map<std::string, std::string> metadata{};
    struct duration
    {
        int64_t hours{0};
        int64_t mins{0};
        int64_t secs{0};
    } duration;
    int start_time_secs{0};
    int64_t bit_rate{0};
    size_t numChapters{0};
    std::vector<StreamInfo> streamInfo{};
};

struct StreamPacket
{
    uint8_t* data{nullptr};
    size_t pkt_size{0};
    int pkt_eos;
};
class IDemuxer
{
public:
    static std::unique_ptr<IDemuxer> create(const std::string& containerPlatform);

    virtual int openContainerParser(const std::string& path) = 0;

    virtual void closeContainerParser() = 0;

    virtual int getContainerInfo(ContainerInfo& containerInfo) = 0;

    virtual int readStreamPacket(StreamPacket& streamPacket) = 0;

    virtual ~IDemuxer() = default;

protected:
    IDemuxer() = default;
    IDemuxer(const IDemuxer&) = delete;
    IDemuxer& operator=(const IDemuxer&) = delete;
};

} // namespace bsp_container

#endif // __IDEMUXER_HPP__