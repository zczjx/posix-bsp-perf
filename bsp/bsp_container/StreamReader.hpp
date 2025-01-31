#ifndef __STREAMREADER_HPP__
#define __STREAMREADER_HPP__

#include "ContainerHeader.hpp"
#include <any>

namespace bsp_container
{
class StreamReader
{
public:
    StreamReader() = default;
    StreamReader(const StreamReader&) = delete;
    StreamReader& operator=(const StreamReader&) = delete;
    virtual ~StreamReader() = default;

    virtual int openStreamReader(const std::string& filename) = 0;
    virtual void closeStreamReader() = 0;
    virtual std::any getStreamParams() = 0;
    virtual int readPacket(StreamPacket& packet) = 0;

};
} // namespace bsp_container

#endif // __STREAMREADER_HPP__