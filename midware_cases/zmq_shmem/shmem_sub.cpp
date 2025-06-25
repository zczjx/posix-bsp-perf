#include <iostream>
#include <string>
#include <thread>
#include <chrono>
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
    parser.addOption("--sync_topic", std::string("ipc:///tmp/bsp_shm_test.sync"), "sync topic for the zmq pub sub");
    parser.addOption("--shm_name", std::string("/bsp_shm_test"), "shm name");
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

    std::string sync_topic;
    parser.getOptionVal("--sync_topic", sync_topic);

    SharedMemSubscriber subscriber(topic, shm_name, slots, single_buffer_size);
    subscriber.replySync(sync_topic);

    std::shared_ptr<uint8_t[]> msg_buffer(new uint8_t[sizeof(SharedMsg)]);
    std::shared_ptr<uint8_t[]> data_buffer{nullptr};
    std::shared_ptr<FILE> out_fp{nullptr};
    while (true)
    {
        subscriber.replySync(sync_topic);
        size_t msg_size = subscriber.receiveMsg(msg_buffer, sizeof(SharedMsg));
        std::cout << "msg_buffer size: " << sizeof(SharedMsg) << std::endl;
        std::cout << "received msg size: " << msg_size << std::endl;

        msgpack::unpacked result = msgpack::unpack(reinterpret_cast<const char*>(msg_buffer.get()), msg_size);
        std::cout << "result.get().as<SharedMsg>() size: " << sizeof(SharedMsg) << std::endl;
        SharedMsg shmem_msg = result.get().as<SharedMsg>();
        std::cout << "shmem_msg done: " << std::endl;

        std::cout << "shmem_msg.msg: " << shmem_msg.msg << std::endl;
        std::cout << "shmem_msg.slot_index: " << shmem_msg.slot_index << std::endl;
        std::cout << "shmem_msg.data_size: " << shmem_msg.data_size << std::endl;
        std::cout << "shmem_msg.pkt_eos: " << shmem_msg.pkt_eos << std::endl;
        std::cout << "shmem_msg.output_file: " << shmem_msg.output_file << std::endl;

        if (shmem_msg.slot_index >= slots)
        {
            std::cerr << "Invalid slot index: " << shmem_msg.slot_index << std::endl;
            continue;
        }

        if (data_buffer == nullptr)
        {
            data_buffer.reset(new uint8_t[shmem_msg.data_size]);
        }
        if (out_fp == nullptr)
        {
            out_fp.reset(fopen(shmem_msg.output_file.c_str(), "wb"));
        }
        subscriber.receiveSharedMemData(data_buffer, shmem_msg.data_size, shmem_msg.slot_index);

        if (shmem_msg.pkt_eos == 1)
        {
            fwrite(data_buffer.get(), 1, shmem_msg.data_size, out_fp.get());
            std::this_thread::sleep_for(std::chrono::seconds(1));
            fflush(out_fp.get());
            out_fp.reset();
            msg_buffer.reset();
            data_buffer.reset();
            break;
        }
        else
        {
            fwrite(data_buffer.get(), 1, shmem_msg.data_size, out_fp.get());
        }
    }

    return 0;
}