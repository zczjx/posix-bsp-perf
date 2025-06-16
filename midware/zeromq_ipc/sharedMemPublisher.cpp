#include "sharedMemPublisher.hpp"
#include <iostream>
#include <cstring>

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

namespace midware
{
namespace zeromq_ipc
{

SharedMemPublisher::SharedMemPublisher(const std::string& topic, const std::string& shm_name,
    size_t slots, size_t single_buffer_size)
    : m_zmq_msg_pub(std::make_shared<ZmqPublisher>(topic))
    , m_shm_name(shm_name)
    , m_slots(slots)
    , m_single_buffer_size(single_buffer_size)
{
    if (m_slots > MAX_BUFFER_SLOTS_COUNT)
    {
        throw std::invalid_argument("slots must be less than or equal to MAX_BUFFER_SLOTS_COUNT");
    }

    if (initSharedMemory())
    {
        std::cout << "SharedMemPublisher initialized successfully" << std::endl;
    }
    else
    {
        std::cerr << "SharedMemPublisher initialization failed" << std::endl;
        throw std::runtime_error("SharedMemPublisher initialization failed");
    }
}

SharedMemPublisher::~SharedMemPublisher()
{
    cleanupSharedMemory();
    m_zmq_msg_pub.reset();
}

bool SharedMemPublisher::initSharedMemory()
{
    size_t total_size = m_single_buffer_size * m_slots;
    m_shm_fd = shm_open(m_shm_name.c_str(), O_CREAT | O_RDWR, 0666);

    if (m_shm_fd == -1)
    {
        std::cerr << "Failed to create shared memory: " << strerror(errno) << std::endl;
        return false;
    }

    if (ftruncate(m_shm_fd, total_size) == -1)
    {
        std::cerr << "Failed to set shared memory size: " << strerror(errno) << std::endl;
        close(m_shm_fd);
        return false;
    }

    m_shm_base_ptr = reinterpret_cast<uint8_t*>(mmap(nullptr, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, m_shm_fd, 0));
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

    resetCurrentSlotIndex();

    std::cout << "Shared memory initialized successfully: " << m_shm_name
                << ", size: " << total_size / (1024*1024) << " MB" << std::endl;
    return true;
}

void SharedMemPublisher::cleanupSharedMemory()
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
    // 删除共享内存对象
    shm_unlink(m_shm_name.c_str());
}

size_t SharedMemPublisher::getFreeSlotIndex()
{
    std::lock_guard<std::mutex> lock(m_shm_mutex);
    size_t slot_index = m_current_slot_index;
    m_current_slot_index = (m_current_slot_index + 1) % m_slots;
    return slot_index;
}

int SharedMemPublisher::publishData(const uint8_t* msg, size_t msg_size,
            const uint8_t* shm_data, size_t slot_index, size_t shm_data_size)
{
    if (slot_index >= m_slots)
    {
        std::cerr << "slot_index out of range" << std::endl;
        return -1;
    }

    if (shm_data_size > m_single_buffer_size)
    {
        std::cerr << "shm_data_size is too large" << std::endl;
        return -1;
    }

    {
        std::lock_guard<std::mutex> lock(m_shm_mutex);

        uint8_t* shm_slot_buffer = m_shm_slots[slot_index];

        if (shm_slot_buffer == nullptr)
        {
            std::cerr << "slot is nullptr" << std::endl;
            return -1;
        }

        std::memcpy(shm_slot_buffer, shm_data, shm_data_size);
    }

    m_zmq_msg_pub->sendData(msg, msg_size);

    return 0;
}

int SharedMemPublisher::publishData(std::shared_ptr<uint8_t[]> msg, size_t msg_size,
            std::shared_ptr<uint8_t[]> shm_data, size_t slot_index, size_t shm_data_size)
{
    return publishData(msg.get(), msg_size, shm_data.get(), slot_index, shm_data_size);
}

} // namespace zeromq_ipc
} // namespace midware