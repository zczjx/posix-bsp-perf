#ifndef BSP_SOCKET_EXCEPTION_HPP
#define BSP_SOCKET_EXCEPTION_HPP

#include <exception>
#include <string>

namespace bsp_sockets {

class BspSocketException : public std::exception
{
public:
    explicit BspSocketException(const std::string& message) : m_msg(message) {}

    virtual const char* what() const noexcept override { return m_msg.c_str();}

private:
    std::string m_msg;
};

}

#endif // BSP_SOCKET_EXCEPTION_HPP
