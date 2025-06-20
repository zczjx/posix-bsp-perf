#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <unistd.h>
#include <shared/ArgParser.hpp>
#include <shared/BspFileUtils.hpp>
#include <msgpack.hpp>
#include <zeromq_ipc/sharedMemPublisher.hpp>
#include "sharedMsg.hpp"

using namespace midware::zeromq_ipc;
using namespace bsp_perf::shared;

int main(int argc, char* argv[])
{
    ArgParser parser("shmem_pub");
    parser.addOption("--topic", std::string("ipc:///tmp/bsp_shm_test.msg"), "topic for the zmq pub sub");
    parser.addOption("--sync_topic", std::string("ipc:///tmp/bsp_shm_test.sync"), "sync topic for the zmq pub sub");
    parser.addOption("--shm_name", std::string("/bsp_shm_test"), "shm name");
    parser.addOption("--slots", size_t(32), "shm slots");
    parser.addOption("--single_buffer_size", int(8192), "shm single buffer size");
    parser.addOption("--file", std::string(""), "file to publish");
    parser.addOption("--msg", std::string("midware_shmem_pub"), "msg to publish");
    parser.parseArgs(argc, argv);

    std::string topic;
    parser.getOptionVal("--topic", topic);

    std::string shm_name;
    parser.getOptionVal("--shm_name", shm_name);

    size_t slots;
    parser.getOptionVal("--slots", slots);

    int single_buffer_size;
    parser.getOptionVal("--single_buffer_size", single_buffer_size);

    std::string file;
    parser.getOptionVal("--file", file);

    std::string msg;
    parser.getOptionVal("--msg", msg);

    std::string sync_topic;
    parser.getOptionVal("--sync_topic", sync_topic);

    SharedMemPublisher shmem_publisher(topic, shm_name, slots, single_buffer_size);
    shmem_publisher.waitSync(sync_topic);
    sleep(1);
    std::shared_ptr<BspFileUtils::FileContext> file_context = BspFileUtils::LoadFileMmap(file);

    const int PKT_CHUNK_SIZE = single_buffer_size;
    uint8_t* pkt_data_start = file_context->data.get();
    uint8_t* pkt_data_end = pkt_data_start + file_context->size;
    SharedMsg shmem_msg;
    shmem_msg.msg = msg;
    shmem_msg.output_file = "output_" + std::string(basename(file.c_str()));

    while (pkt_data_start < pkt_data_end)
    {
        shmem_publisher.waitSync(sync_topic);
        msgpack::sbuffer sbuf;
        int pkt_eos = 0;
        int chunk_size = PKT_CHUNK_SIZE;

        if (pkt_data_start + chunk_size >= pkt_data_end)
        {
            pkt_eos = 1;
            chunk_size = pkt_data_end - pkt_data_start;
        }

        shmem_msg.slot_index = shmem_publisher.getFreeSlotIndex();
        shmem_msg.data_size = chunk_size;
        shmem_msg.pkt_eos = pkt_eos;
        msgpack::pack(sbuf, shmem_msg);
        shmem_publisher.publishData(reinterpret_cast<const uint8_t*>(sbuf.data()), sbuf.size(),
            pkt_data_start, shmem_msg.slot_index, shmem_msg.data_size);
        std::cout << "published data to slot: " << shmem_msg.slot_index << std::endl;

        pkt_data_start += chunk_size;
    }

    return 0;
}