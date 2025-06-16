#include "sharedMemSubscriber.hpp"
#include <iostream>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

namespace midware
{
namespace zeromq_ipc
{

SharedMemSubscriber::SharedMemSubscriber(const std::string& topic, const std::string& shm_name,
    size_t slots, size_t single_buffer_size)
    : m_zmq_msg_sub(std::make_shared<ZmqSubscriber>(topic))
    , m_shm_name(shm_name)
    , m_slots(slots)
    , m_single_buffer_size(single_buffer_size)
{
    if (connectToSharedMemory())
    {
        std::cout << "SharedMemSubscriber initialized successfully" << std::endl;
    }
    else
    {
        std::cerr << "SharedMemSubscriber initialization failed" << std::endl;
        throw std::runtime_error("SharedMemSubscriber initialization failed");
    }
}

SharedMemSubscriber::~SharedMemSubscriber()
{
    cleanupSharedMemory();
    m_zmq_msg_sub.reset();
}

bool SharedMemSubscriber::connectToSharedMemory()
{
    size_t total_size = m_single_buffer_size * m_slots;
    m_shm_fd = shm_open(m_shm_name.c_str(), O_RDONLY, 0666);

    if (m_shm_fd == -1)
    {
        std::cerr << "Failed to create shared memory: " << strerror(errno) << std::endl;
        return false;
    }

    m_shm_base_ptr = reinterpret_cast<uint8_t*>(mmap(nullptr, total_size, PROT_READ, MAP_SHARED, m_shm_fd, 0));

    if (m_shm_base_ptr == MAP_FAILED)
    {
        std::cerr << "Failed to map shared memory: " << strerror(errno) << std::endl;
        close(m_shm_fd);
        return false;
    }

    m_shm_slots.resize(m_slots);
    for (size_t i = 0; i < m_slots; ++i)
    {
        m_shm_slots[i] = m_shm_base_ptr + i * m_single_buffer_size;
    }

    std::cout << "Shared memory connected successfully: " << m_shm_name
                << ", size: " << total_size / (1024*1024) << " MB" << std::endl;
    return true;
}

size_t SharedMemSubscriber::receiveMsg(uint8_t* msg_buffer, size_t msg_size)
{
    return m_zmq_msg_sub->receiveData(msg_buffer, msg_size);
}

size_t SharedMemSubscriber::receiveMsg(std::shared_ptr<uint8_t[]> msg_buffer, size_t msg_size)
{
    return m_zmq_msg_sub->receiveData(msg_buffer, msg_size);
}

int SharedMemSubscriber::receiveSharedMemData(uint8_t* data_buffer, size_t data_size, size_t slot_index)
{
    if (slot_index >= m_slots)
    {
        std::cerr << "Invalid slot index: " << slot_index << std::endl;
        return -1;
    }

    std::lock_guard<std::mutex> lock(m_shm_mutex);
    size_t actual_bytes = std::min(m_single_buffer_size, data_size);
    std::memcpy(data_buffer, m_shm_slots[slot_index], actual_bytes);

    return actual_bytes;
}

int SharedMemSubscriber::receiveSharedMemData(std::shared_ptr<uint8_t[]> data_buffer, size_t data_size, size_t slot_index)
{
    return receiveSharedMemData(data_buffer.get(), data_size, slot_index);
}

void SharedMemSubscriber::cleanupSharedMemory()
{
    if (m_shm_base_ptr != nullptr && m_shm_base_ptr != MAP_FAILED)
    {
        size_t total_size = m_single_buffer_size * m_slots;
        munmap(m_shm_base_ptr, total_size);
        m_shm_base_ptr = nullptr;
    }

    if (m_shm_fd != -1)
    {
        close(m_shm_fd);
        m_shm_fd = -1;
    }

    m_shm_slots.clear();
    m_shm_slots.shrink_to_fit();
}

} // namespace zeromq_ipc
} // namespace midware
