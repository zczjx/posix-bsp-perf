#ifndef _SHARED_MSG_HPP_
#define _SHARED_MSG_HPP_

#include <msgpack.hpp>
#include <string>
#include <cstdint>

struct SharedMsg
{
    std::string msg;
    std::string output_file;
    size_t slot_index{0};
    size_t data_size{0};
    int pkt_eos{0};
    MSGPACK_DEFINE(msg, output_file, slot_index, data_size, pkt_eos);
};

#endif // _SHARED_MSG_HPP_