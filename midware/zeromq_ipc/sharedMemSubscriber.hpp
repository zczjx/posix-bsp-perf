#ifndef __SHARED_MEM_SUBSCRIBER_HPP__
#define __SHARED_MEM_SUBSCRIBER_HPP__

#include "zmqSubscriber.hpp"
#include <memory>
#include <string>
#include <mutex>

namespace midware
{
namespace zeromq_ipc
{

class SharedMemSubscriber
{
public:
    /**
     * @brief Constructor for SharedMemSubscriber
     * @param topic The ZeroMQ topic to subscribe to for receiving messages
     * @param shm_name The name of the shared memory segment to connect to
     * @param slots Number of buffer slots in the shared memory (max 8)
     * @param single_buffer_size Size of each slot's buffer in bytes
     */
    explicit SharedMemSubscriber(const std::string& topic, const std::string& shm_name,
                            size_t slots, size_t single_buffer_size);

    virtual ~SharedMemSubscriber();

    /**
     * @brief Receive and process message from ZeroMQ
     *
     * This function receives a message from ZeroMQ and deserializes it to get the shared memory slot ID.
     * The message typically contains metadata about the data stored in shared memory.
     * After receiving the message, you can use receiveSharedMemData() to read the actual data
     * from the shared memory slot.
     *
     * @param msg_buffer Buffer to store the received message
     * @param msg_size Size of the message buffer
     * @return size_t the number of bytes received
     */
    size_t receiveMsg(uint8_t* msg_buffer, size_t msg_size);

    size_t receiveMsg(std::shared_ptr<uint8_t[]> msg_buffer, size_t msg_size);

    /**
     * @brief Receive shared memory data from the specified slot
     *
     * This function reads the data from the shared memory slot specified by slot_index.
     * The data is copied into the provided buffer.
     *
     * @param data_buffer Buffer to store the received data
     * @param data_size Size of the data buffer
     * @param slot_index Index of the shared memory slot to read data from，
     * the slot_index should get from the msg received by receiveMsg()
     * @return int 0 on success, -1 on failure
     */
    int receiveSharedMemData(uint8_t* data_buffer, size_t data_size, size_t slot_index);

    int receiveSharedMemData(std::shared_ptr<uint8_t[]> data_buffer, size_t data_size, size_t slot_index);

    const std::string& getSharedMemoryName() const { return m_shm_name; }

    int replySync(const std::string& sync_topic)
    {
        return m_zmq_msg_sub->replySync(sync_topic);
    }

private:

    bool connectToSharedMemory();

    void cleanupSharedMemory();

private:

    size_t m_slots{0};
    size_t m_single_buffer_size{0};
    std::string m_shm_name{};                              // shared memory name

    int m_shm_fd{-1};                                        // shared memory file descriptor
    uint8_t* m_shm_base_ptr{nullptr};                             // shared memory pointer
    std::vector<uint8_t*> m_shm_slots;
    size_t m_current_slot_index{0};
    std::mutex m_shm_mutex{};

    std::shared_ptr<ZmqSubscriber> m_zmq_msg_sub;
};

} // namespace zeromq_ipc
} // namespace midware

#endif // __SHARED_MEM_SUBSCRIBER_HPP__