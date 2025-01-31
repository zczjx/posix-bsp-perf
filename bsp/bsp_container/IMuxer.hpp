#ifndef __IMUXER_HPP__
#define __IMUXER_HPP__

#include <memory>
#include <string>
#include <functional>
#include "ContainerHeader.hpp"
#include "StreamReader.hpp"

namespace bsp_container
{
class IMuxer
{
public:

    static std::unique_ptr<IMuxer> create(const std::string& codecPlatform);

    virtual ~IMuxer() = default;

    struct MuxConfig
    {
        bool ts_recreate{true};
        float video_fps{29.97};
    };

    virtual int openContainerMux(const std::string& path, MuxConfig& config) = 0;

    virtual void closeContainerMux() = 0;

    /**
     * @brief Adds a stream to the muxer.
     *
     * @param streamInfo Reference to a StreamInfo object containing the stream details.
     * @return int The stream ID of the added stream.
     */
    virtual int addStream(StreamInfo& streamInfo) = 0;

    /**
     * @brief Adds a stream to the muxer.
     *
     * @param strReader Shared pointer to a StreamReader object.
     * @return int The stream ID of the added stream.
     */
    virtual int addStream(std::shared_ptr<StreamReader> strReader) = 0;

    virtual int writeStreamPacket(StreamPacket& streamPacket) = 0;

    virtual std::shared_ptr<StreamReader> getStreamReader(const std::string& filename) = 0;

    virtual int endStreamMux() = 0;



protected:
    IMuxer() = default;
    IMuxer(const IMuxer&) = delete;
    IMuxer& operator=(const IMuxer&) = delete;
};

} // namespace bsp_container

#endif // __IMUXER_HPP__