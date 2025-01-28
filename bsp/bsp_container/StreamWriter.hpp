#ifndef __STREAM_WRITER_HPP__
#define __STREAM_WRITER_HPP__

#include "ContainerHeader.hpp"
#include <any>

namespace bsp_container
{
class StreamWriter
{
public:
    StreamWriter() = default;
    StreamWriter(const StreamWriter&) = delete;
    StreamWriter& operator=(const StreamWriter&) = delete;
    virtual ~StreamWriter() = default;

    virtual int openStreamWriter(const std::string& filename, std::any params) = 0;
    virtual void closeStreamWriter() = 0;
    virtual int writeHeader() = 0;
    virtual int writePacket(const StreamPacket& packet) = 0;
    virtual int writeTrailer() = 0;

};
} // namespace bsp_container

#endif // __STREAM_WRITER_HPP__