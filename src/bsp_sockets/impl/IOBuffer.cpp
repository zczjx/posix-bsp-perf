#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include "IOBuffer.hpp"

namespace bsp_sockets
{
using namespace bsp_perf::shared;

static constexpr unsigned int EXTRA_MEM_LIMIT = 5 * 1024 * 1024; //unit is K, so EXTRA_MEM_LIMIT = 5GB

int InputBufferQueue::readData(int fd)
{
    //一次性读出来所有数据
    int rn, ret;

    if (isFull())
    {
        m_logger->printStdoutLog(BspLogger::LogLevel::Error, "InputBufferQueue is full");
        return -1;
    }

    if (::ioctl(fd, FIONREAD, &rn) == -1)
    {
        m_logger->printStdoutLog(BspLogger::LogLevel::Error, "ioctl FIONREAD");
        return -1;
    }
    std::vector<uint8_t> buffer(rn);
    do
    {
        ret = ::read(fd, buffer.data(), rn);
    }
    while (ret == -1 && errno == EINTR);

    if ((ret > 0) && (ret == rn))
    {
        appendBuffer(buffer);
    }
    else
    {
        m_logger->printStdoutLog(BspLogger::LogLevel::Error, "readData error");
    }

    return ret;
}

int OutputBufferQueue::sendData(const std::vector<uint8_t>& buffer)
{
    appendBuffer(buffer);
    return 0;

}

int OutputBufferQueue::writeFd(int fd)
{
    if (getBuffersCount() == 0)
    {
        m_logger->printStdoutLog(BspLogger::LogLevel::Debug, "OutputBufferQueue is empty");
        return 0;
    }

    auto& buffer = getFrontBuffer();
    int writed;
    do
    {
        writed = ::write(fd, buffer.data(), buffer.size());
    }
    while (writed == -1 && errno == EINTR);

    if (writed >= 0)
    {
        popBuffer();
    }

    return writed;
}

}
