
/*
MIT License

Copyright (c) 2024 Clarence Zhou<287334895@qq.com> and contributors.

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

#ifndef BASE_PERF_CASE_HPP
#define BASE_PERF_CASE_HPP

#include <string>
#include <iostream>
#include <shared/ArgParser.hpp>

namespace bsp_perf {
namespace common {

class BasePerfCase {
public:

    static constexpr char LOG_TAG[] {"[BasePerfCase]: "};
    BasePerfCase(bsp_perf::shared::ArgParser&& args); // 添加构造函数
    BasePerfCase(const BasePerfCase&) = delete; // 删除拷贝构造函数
    BasePerfCase& operator=(const BasePerfCase&) = delete; // 删除拷贝赋值运算符
    BasePerfCase(BasePerfCase&&) = delete; // 删除移动构造函数
    BasePerfCase& operator=(BasePerfCase&&) = delete; // 删除移动赋值运算符
    virtual ~BasePerfCase() = default; // 设置析构函数为虚函数

    virtual void run(int32_t cycles = 1) final;

protected:
    bsp_perf::shared::ArgParser& getArgs() { return m_args; } // 添加成员函数 getArgs()

private:
    virtual void onInit() = 0; // 添加纯虚函数 onInit()
    virtual void onProcess() = 0; // 添加纯虚函数 onProcess()
    virtual void onRender() = 0; // 添加纯虚函数 onRender()
    virtual void onRelease() = 0; // 添加纯虚函数 onRelease()

private:
    bsp_perf::shared::ArgParser m_args; // 添加成员变量 m_args

};

} // namespace common
} // namespace bsp_perf

#endif // BASE_PERF_CASE_HPP

