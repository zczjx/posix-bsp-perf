#ifndef IDEMUXER_HPP
#define IDEMUXER_HPP

class IDemuxer {
public:
    virtual ~IDemuxer() = default;
    virtual void demux() = 0;
};

IDemuxer* createDemuxer();

#endif // IDEMUXER_HPP