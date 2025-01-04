#ifndef __IO_BUFFER_H__
#define __IO_BUFFER_H__

#include <queue>
#include <string>
#include <mutex>
#include <vector>
#include <utility>
#include <cstdint>

#include <shared/BspLogger.hpp>


namespace bsp_sockets
{
using namespace bsp_perf::shared;
class IOBufferQueue
{
public:
    constexpr static size_t MAX_BUFFER_NUM = 256;

    IOBufferQueue() = default;
    virtual ~IOBufferQueue() = default;
    IOBufferQueue(const IOBufferQueue&) = delete;
    IOBufferQueue& operator=(const IOBufferQueue&) = delete;
    IOBufferQueue(IOBufferQueue&&) = delete;
    IOBufferQueue& operator=(IOBufferQueue&&) = delete;

    virtual int getBuffersCount()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_buffers.size();
    }

    virtual bool isFull()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_buffers.size() >= MAX_BUFFER_NUM;
    }

    virtual bool isEmpty()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_buffers.empty();
    }

    virtual void appendBuffer(const std::vector<uint8_t>& buffer)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_buffers.push(buffer);
    }
    virtual std::vector<uint8_t>& getFrontBuffer()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_buffers.front();
    }
    virtual void popBuffer()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_buffers.pop();
    }
    virtual void clearBuffers()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::queue<std::vector<uint8_t>> empty;

        std::swap(m_buffers, empty);
    }

private:
    std::queue<std::vector<uint8_t>> m_buffers{};
    std::mutex m_mutex{};
};

class InputBufferQueue: public IOBufferQueue
{
public:
    InputBufferQueue(): m_logger(std::make_unique<BspLogger>("InputBufferQueue")) {}
    ~InputBufferQueue() = default;
    InputBufferQueue(const InputBufferQueue&) = delete;
    InputBufferQueue& operator=(const InputBufferQueue&) = delete;
    InputBufferQueue(InputBufferQueue&&) = delete;
    InputBufferQueue& operator=(InputBufferQueue&&) = delete;

    int readData(int fd);

private:
    std::unique_ptr<BspLogger> m_logger;
};


class OutputBufferQueue: public IOBufferQueue
{
public:
    OutputBufferQueue(): m_logger(std::make_unique<BspLogger>("OutputBufferQueue")) {}
    ~OutputBufferQueue() = default;
    OutputBufferQueue(const OutputBufferQueue&) = delete;
    OutputBufferQueue& operator=(const OutputBufferQueue&) = delete;
    OutputBufferQueue(OutputBufferQueue&&) = delete;
    OutputBufferQueue& operator=(OutputBufferQueue&&) = delete;

    int sendData(const std::vector<uint8_t>& buffer);
    int writeFd(int fd);

private:
    std::unique_ptr<BspLogger> m_logger;
};


} // namespace bsp_sockets

#endif // __IO_BUFFER_H__
