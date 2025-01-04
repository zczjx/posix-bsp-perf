/*
MIT License

Copyright (c) <2024> Clarence Zhou<287334895@qq.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef __ISOCKET_HELPER_HPP__
#define __ISOCKET_HELPER_HPP__

#include <stdint.h>
#include <any>
#include <vector>
#include "MsgHead.hpp"

namespace bsp_sockets
{
class ISocketHelper
{
public:
    ISocketHelper() = default;
    virtual ~ISocketHelper() = default;
    ISocketHelper(const ISocketHelper&) = delete;
    ISocketHelper& operator=(const ISocketHelper&) = delete;
    ISocketHelper(ISocketHelper&&) = delete;
    ISocketHelper& operator=(ISocketHelper&&) = delete;

    virtual int sendData(size_t cmd_id, std::vector<uint8_t>& data) = 0;
    virtual int sendData(std::string& cmd_name, std::vector<uint8_t>& data)
    {
        return sendData(genCmdId(cmd_name), data);
    }

    virtual int getFd() = 0;

};
} // namespace bsp_sockets

#endif // __ISOCKET_HELPER_HPP__

