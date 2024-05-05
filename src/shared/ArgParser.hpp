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

#ifndef ARG_PARSER_HPP
#define ARG_PARSER_HPP

#include <CLI/CLI.hpp>
#include <memory>
#include <string>

// Your code here
namespace bsp_perf {
namespace shared {

class ArgParser {
public:
    ArgParser(const std::string& description = ""):
        parser(std::make_unique<CLI::App>(description)) {}
    virtual ~ArgParser() = default;
    ArgParser(const ArgParser&) = delete;
    ArgParser& operator=(const ArgParser&) = delete;
    ArgParser(ArgParser&&) = delete;
    ArgParser& operator=(ArgParser&&) = delete;

    template <typename T>
    void addOption(const std::string& name, T defaultVal, const std::string& description = "")
    {
        parser->add_option(name, defaultVal, description);
    }

    template <typename T>
    void addFlag(const std::string& name, T defaultVal, const std::string& description = "")
    {
        parser->add_flag(name, defaultVal, description);
    }

    void parseArgs(int argc, char* argv[])
    {
        parser->parse(argc, argv);
    }


private:
    std::unique_ptr<CLI::App> parser; // Change unique_ptr to shared_ptr for CLI::App
};


} // namespace shared
} // namespace bsp_perf

#endif // ARG_PARSER_HPP

