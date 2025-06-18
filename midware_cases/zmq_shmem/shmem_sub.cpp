#include <iostream>
#include <string>
#include <shared/ArgParser.hpp>
#include <shared/BspFileUtils.hpp>
#include <msgpack.hpp>
#include <zeromq_ipc/sharedMemSubscriber.hpp>
#include "sharedMsg.hpp"

using namespace midware::zeromq_ipc;
using namespace bsp_perf::shared;

int main(int argc, char* argv[])
{
    ArgParser parser("shmem_sub");
    parser.addOption("--topic", std::string("ipc:///tmp/bsp_shm_test.msg"), "topic for the zmq pub sub");
    parser.addOption("--shm_name", std::string("/tmp/bsp_shm_test.data"), "shm name");
    parser.addOption("--slots", size_t(32), "shm slots");
    parser.addOption("--single_buffer_size", int(8192), "shm single buffer size");
    parser.parseArgs(argc, argv);

    std::string topic;
    parser.getOptionVal("--topic", topic);

    std::string shm_name;
    parser.getOptionVal("--shm_name", shm_name);

    size_t slots;
    parser.getOptionVal("--slots", slots);

    int single_buffer_size;
    parser.getOptionVal("--single_buffer_size", single_buffer_size);

    SharedMemSubscriber subscriber(topic, shm_name, slots, single_buffer_size);
    msgpack::sbuffer sbuf(sizeof(SharedMsg));
    uint8_t* data_buffer{nullptr};
    std::shared_ptr<FILE> out_fp{nullptr};
    while (true)
    {
        size_t msg_size = subscriber.receiveMsg(reinterpret_cast<uint8_t*>(sbuf.data()), sbuf.size());
        msgpack::unpacked result = msgpack::unpack(sbuf.data(), msg_size);
        SharedMsg shmem_msg = result.get().as<SharedMsg>();

        if (shmem_msg.slot_index >= slots)
        {
            std::cerr << "Invalid slot index: " << shmem_msg.slot_index << std::endl;
            continue;
        }

        if (data_buffer == nullptr)
        {
            data_buffer = new uint8_t[shmem_msg.data_size];
        }
        if (out_fp == nullptr)
        {
            out_fp.reset(fopen(shmem_msg.output_file.c_str(), "wb"));
        }
        subscriber.receiveSharedMemData(data_buffer, shmem_msg.data_size, shmem_msg.slot_index);

        if (shmem_msg.pkt_eos == 1)
        {
            fwrite(data_buffer, 1, shmem_msg.data_size, out_fp.get());
            fflush(out_fp.get());
            fclose(out_fp.get());
            break;
        }
        else
        {
            fwrite(data_buffer, 1, shmem_msg.data_size, out_fp.get());
        }
    }

    return 0;
}