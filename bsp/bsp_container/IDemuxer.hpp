#ifndef __IDEMUXER_HPP__
#define __IDEMUXER_HPP__

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "ContainerHeader.hpp"
#include "StreamWriter.hpp"

namespace bsp_container
{
class IDemuxer
{
public:
    static std::unique_ptr<IDemuxer> create(const std::string& containerPlatform);

    virtual int openContainerDemux(const std::string& path) = 0;

    virtual void closeContainerDemux() = 0;

    virtual int getContainerInfo(ContainerInfo& containerInfo) = 0;

    virtual int readStreamPacket(StreamPacket& streamPacket) = 0;

    virtual int seekStreamFrame(int stream_index, int64_t timestamp) = 0;

    virtual std::shared_ptr<StreamWriter> getStreamWriter(int stream_index, const std::string& filename) = 0;

    virtual ~IDemuxer() = default;

protected:
    IDemuxer() = default;
    IDemuxer(const IDemuxer&) = delete;
    IDemuxer& operator=(const IDemuxer&) = delete;
};

} // namespace bsp_container

#endif // __IDEMUXER_HPP__