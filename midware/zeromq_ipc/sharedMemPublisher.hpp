#ifndef _SHARED_MEM_PUBLISHER_HPP_
#define _SHARED_MEM_PUBLISHER_HPP_

#include "zmqPublisher.hpp"
#include <memory>
#include <string>
#include <mutex>

namespace midware
{
namespace zeromq_ipc
{

class SharedMemPublisher
{
public:
    /**
     * @brief Constructor for SharedMemPublisher
     * @param topic The ZeroMQ topic to publish messages to
     * @param shm_name The name of the shared memory segment to create
     * @param slots Number of buffer slots in the shared memory (max 8)
     * @param single_buffer_size Size of each slot's buffer in bytes
     */
    explicit SharedMemPublisher(const std::string& topic, const std::string& shm_name,
                            size_t slots, size_t single_buffer_size);

    virtual ~SharedMemPublisher();

    /**
     * @brief Publish data to both ZeroMQ and shared memory
     *
     * This function publishes data in two parts:
     * 1. The actual data is written to shared memory at the specified slot
     * 2. A ZeroMQ message is sent containing metadata (slot index) that subscribers
     *    can use to locate and read the data from shared memory
     *
     * @param msg Pointer to the metadata message to be sent via ZeroMQ
     * @param msg_size Size of the metadata message in bytes
     * @param shm_data Pointer to the actual data to be written to shared memory
     * @param slot_index Index of the shared memory slot to write data to
     * @param shm_data_size Size of the data to be written to shared memory
     * @return 0 on success, -1 on failure
     */
    int publishData(const uint8_t* msg, size_t msg_size,
            const uint8_t* shm_data, size_t slot_index, size_t shm_data_size);

    int publishData(std::shared_ptr<uint8_t[]> msg, size_t msg_size,
            std::shared_ptr<uint8_t[]> shm_data, size_t slot_index, size_t shm_data_size);

    size_t getFreeSlotIndex();

    size_t getMaxSlotsCount() const { return MAX_BUFFER_SLOTS_COUNT; }

    const std::string& getSharedMemoryName() const { return m_shm_name; }

    size_t getSingleBufferMaxSize() const { return m_single_buffer_size; }

    size_t getCurrentSlotIndex()
    {
        std::lock_guard<std::mutex> lock(m_shm_mutex);
        return m_current_slot_index;
    }

    void resetCurrentSlotIndex()
    {
        std::lock_guard<std::mutex> lock(m_shm_mutex);
        m_current_slot_index = 0;
    }

    int waitSync(const std::string& sync_topic)
    {
        return m_zmq_msg_pub->waitSync(sync_topic);
    }

private:

    bool initSharedMemory();

    void cleanupSharedMemory();

private:

    static constexpr size_t MAX_BUFFER_SLOTS_COUNT = 32;       // number of buffer slots
    size_t m_slots{0};
    size_t m_single_buffer_size{0};
    std::string m_shm_name{};                              // shared memory name
    int m_shm_fd{-1};                                        // shared memory file descriptor
    uint8_t* m_shm_base_ptr{nullptr};                             // shared memory pointer
    std::vector<uint8_t*> m_shm_slots;
    size_t m_current_slot_index{0};
    std::mutex m_shm_mutex{};

    std::shared_ptr<ZmqPublisher> m_zmq_msg_pub;
};

} // namespace zeromq_ipc
} // namespace midware


#endif // _SHARED_MEM_PUBLISHER_HPP_