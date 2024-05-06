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
    ArgParser(ArgParser&&) = default;
    ArgParser& operator=(ArgParser&&) = default;

    template <typename T>
    void addOption(const std::string& name, const T defaultVal, const std::string& description = "")
    {
        CLI::Option *opt = parser->add_option(name, description);
        if (opt)
        {
            opt->default_val(defaultVal);
        }
    }

    template <typename T>
    void getOptionVal(const std::string& option_name, T& ret)
    {
        CLI::Option *opt = parser->get_option(option_name);
        if (opt)
        {
            ret = opt->as<T>();
        }
    }

    void addFlag(const std::string& flag_name, const bool defaultVal, const std::string& description = "")
    {
        CLI::Option *flag = parser->add_flag(flag_name, description);
        if (flag)
        {
            flag->default_val(defaultVal);
        }
    }

    bool getFlagVal(const std::string& flag_name)
    {
        return parser->get_option(flag_name)->as<bool>();
    }

    void parseArgs(int argc, char* argv[]) noexcept
    {
        try
        {
            parser->parse(argc, argv);
        }
        catch (const CLI::ExtrasError& e)
        {
            std::cerr << "Exception caught: " << e.what() << std::endl;
        }
    }


private:
    std::unique_ptr<CLI::App> parser; // Change unique_ptr to shared_ptr for CLI::App
};


} // namespace shared
} // namespace bsp_perf

#endif // ARG_PARSER_HPP

