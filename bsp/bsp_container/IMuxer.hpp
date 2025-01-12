#ifndef __IMUXER_HPP__
#define __IMUXER_HPP__

#include <memory>
#include <string>
#include <functional>
#include "ContainerHeader.hpp"

namespace bsp_container
{
class IMuxer
{
public:

    static std::unique_ptr<IMuxer> create(const std::string& codecPlatform);
    virtual ~IMuxer() = default;

    virtual int openContainerMux(const std::string& path) = 0;
    virtual void closeContainerMux() = 0;

    virtual int addStream(StreamInfo& streamInfo) = 0;

    virtual int writeStreamPacket(StreamPacket& streamPacket) = 0;



protected:
    IMuxer() = default;
    IMuxer(const IMuxer&) = delete;
    IMuxer& operator=(const IMuxer&) = delete;
};

} // namespace bsp_container

#endif // __IMUXER_HPP__