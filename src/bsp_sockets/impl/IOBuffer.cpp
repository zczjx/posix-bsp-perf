#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include "BspSocketException.hpp"
#include "IOBuffer.hpp"

namespace bsp_sockets
{
using namespace bsp_perf::shared;

static constexpr unsigned int EXTRA_MEM_LIMIT = 5 * 1024 * 1024; //unit is K, so EXTRA_MEM_LIMIT = 5GB

int InputBufferQueue::readData(int fd)
{
    //一次性读出来所有数据
    int rn, ret;

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
        throw BspSocketException("readData error");
    }

    return ret;

}

int OutputBufferQueue::sendData(const std::vector<uint8_t>& buffer)
{
    if (!_buf)
    {
        _buf = buffer_pool::ins()->alloc(datlen);
        if (!_buf)
        {
            error_log("no idle for alloc io_buffer");
            return -1;
        }
    }
    else
    {
        assert(_buf->head == 0);
        if (_buf->capacity - _buf->length < datlen)
        {
            //get new
            io_buffer* new_buf = buffer_pool::ins()->alloc(datlen + _buf->length);
            if (!new_buf)
            {
                error_log("no idle for alloc io_buffer");
                return -1;
            }
            new_buf->copy(_buf);
            buffer_pool::ins()->revert(_buf);
            _buf = new_buf;
        }
    }

    ::memcpy(_buf->data + _buf->length, data, datlen);
    _buf->length += datlen;
    return 0;

}

int OutputBufferQueue::writeFd(int fd)
{
    assert(_buf && _buf->head == 0);
    int writed;
    do
    {
        writed = ::write(fd, _buf->data, _buf->length);
    } while (writed == -1 && errno == EINTR);
    if (writed > 0)
    {
        _buf->pop(writed);
        _buf->adjust();
    }
    if (writed == -1 && errno == EAGAIN)
    {
        writed = 0;//不是错误，仅返回为0表示此时不可继续写
    }
    return writed;
}

}
