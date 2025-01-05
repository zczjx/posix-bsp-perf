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

#ifndef __ARG_PARSER_HPP__
#define __ARG_PARSER_HPP__

#include <CLI/CLI.hpp>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace bsp_perf {
namespace shared {
class ArgParser
{
public:
    ArgParser(const std::string& description = ""): m_parser{std::make_unique<CLI::App>(description)} {}
    virtual ~ArgParser() = default;
    ArgParser(const ArgParser&) = delete;
    ArgParser& operator=(const ArgParser&) = delete;
    ArgParser(ArgParser&&) = default;
    ArgParser& operator=(ArgParser&&) = default;

    void setConfig(const std::string& config_name, const std::string& default_filename = "", const std::string& help_message = "Read an ini file", const bool config_required = false)
    {
        m_parser->set_config(config_name, default_filename, help_message, config_required);
    }

    template <typename T>
    void addOption(const std::string& name, const T defaultVal, const std::string& description = "")
    {
        CLI::Option *opt = m_parser->add_option(name, description);
        if (opt)
        {
            opt->default_val(defaultVal);
        }
    }

    template <typename T>
    void getOptionVal(const std::string& option_name, T& ret)
    {
        CLI::Option *opt = m_parser->get_option(option_name);
        if (opt)
        {
            ret = opt->as<T>();
        }
    }

    void addFlag(const std::string& flag_name, const bool defaultVal, const std::string& description = "")
    {
        CLI::Option *flag = m_parser->add_flag(flag_name, description);
        if (flag)
        {
            flag->default_val(defaultVal);
        }
    }

    bool getFlagVal(const std::string& flag_name)
    {
        return m_parser->get_option(flag_name)->as<bool>();
    }

    auto parseArgs(int argc, char* argv[]) noexcept
    {
        try
        {
            m_parser->parse(argc, argv);
        }
        catch (const CLI::ParseError &e)
        {
             return m_parser->exit(e);
        }
        return 0;
    }

    void addSubCmd(const std::string& name, const std::string& description = "")
    {
        m_subcmdMap[name] = m_parser->add_subcommand(name, description);
    }

    template <typename T>
    void addSubOption(const std::string& subcmd, const std::string& name, const T defaultVal, const std::string& description = "")
    {

        auto it = m_subcmdMap.find(subcmd);

        if(it == m_subcmdMap.end())
        {
            addSubCmd(subcmd);
        }
        auto subcmd_ptr = m_subcmdMap[subcmd];
        CLI::Option *opt = subcmd_ptr->add_option(name, description);
        if (opt)
        {
            opt->default_val(defaultVal);
        }
    }

    template <typename T>
    void getSubOptionVal(const std::string& subcmd, const std::string& option_name, T& ret)
    {
        auto it = m_subcmdMap.find(subcmd);
        if(it == m_subcmdMap.end())
        {
            return;
        }

        auto subcmd_ptr = m_subcmdMap[subcmd];
        CLI::Option *opt = subcmd_ptr->get_option(option_name);

        if (opt)
        {
            ret = opt->as<T>();
        }
    }

    void getOptionSplitStrList(const std::string& subcmd, const std::string& option_name, std::vector<std::string>& ret)
    {
        std::string ids_input;
        getSubOptionVal(subcmd, option_name, ids_input);
        std::stringstream ss(ids_input);
        std::string item;
        ret.clear();

        auto trim = [](std::string_view str) {
            while (!str.empty() && std::isspace(str.front())) str.remove_prefix(1);
            while (!str.empty() && std::isspace(str.back())) str.remove_suffix(1);
            return str;
        };

        while (std::getline(ss, item, ';'))
        {
            ret.push_back(std::string(trim(item)));
        }
    }

    void addSubFlag(const std::string& subcmd, const std::string& flag_name, const bool defaultVal, const std::string& description = "")
    {
        auto it = m_subcmdMap.find(subcmd);

        if(it == m_subcmdMap.end())
        {
            addSubCmd(subcmd);
        }
        auto subcmd_ptr = m_subcmdMap[subcmd];
        CLI::Option *flag = subcmd_ptr->add_flag(flag_name, description);

        if (flag)
        {
            flag->default_val(defaultVal);
        }
    }

    bool getSubFlagVal(const std::string& subcmd, const std::string& flag_name)
    {
        auto it = m_subcmdMap.find(subcmd);
        if(it == m_subcmdMap.end())
        {
            return false;
        }
        auto subcmd_ptr = m_subcmdMap[subcmd];
        return subcmd_ptr->get_option(flag_name)->as<bool>();
    }


private:
    std::unique_ptr<CLI::App> m_parser; // Change unique_ptr to shared_ptr for CLI::App
    std::unordered_map<std::string, CLI::App*> m_subcmdMap;
};


} // namespace shared
} // namespace bsp_perf

#endif // __ARG_PARSER_HPP__

