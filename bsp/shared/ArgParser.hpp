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
/**
 * @brief ArgParser 类,封装了 CLI11 库,用于解析命令行参数。
 */
class ArgParser
{
public:
    /**
     * @brief 构造函数。
     * @param description 应用程序的描述。
     */
    ArgParser(const std::string& description = ""): m_parser{std::make_unique<CLI::App>(description)} {}

    virtual ~ArgParser() = default;
    ArgParser(const ArgParser&) = delete;
    ArgParser& operator=(const ArgParser&) = delete;
    ArgParser(ArgParser&&) = default;
    ArgParser& operator=(ArgParser&&) = default;


    /**
     * @brief 设置 INI 配置文件的选项。
     * @param config_name 配置选项的名称。
     * @param default_filename 默认配置文件的路径。
     * @param help_message 帮助信息。
     * @param config_required 是否必须提供配置文件。
     */
    void setConfig(const std::string& config_name, const std::string& default_filename = "", const std::string& help_message = "Read an ini file", const bool config_required = false)
    {
        m_parser->set_config(config_name, default_filename, help_message, config_required);
    }

    /**
     * @brief 添加一个命令行选项。
     * @param name 选项的名称(例如,"--input")。
     * @param defaultVal 选项的默认值。
     * @param description 选项的描述。
     */
    template <typename T>
    void addOption(const std::string& name, const T defaultVal, const std::string& description = "")
    {
        CLI::Option *opt = m_parser->add_option(name, description);
        if (opt)
        {
            opt->default_val(defaultVal);
        }
    }

    /**
     * @brief 获取指定选项的值。
     * @param option_name 选项的名称。
     * @param[out] ret 用于存储选项值的变量。
     */
    template <typename T>
    void getOptionVal(const std::string& option_name, T& ret)
    {
        CLI::Option *opt = m_parser->get_option(option_name);
        if (opt)
        {
            ret = opt->as<T>();
        }
    }

    /**
     * @brief 添加一个标志(flag),标志是没有值的选项,只有存在或不存在两种状态(true/false)。
     * @param flag_name 标志的名称(例如,"--verbose")。
     * @param defaultVal 标志的默认值(true 或 false)。
     * @param description 标志的描述。
     */
    void addFlag(const std::string& flag_name, const bool defaultVal, const std::string& description = "")
    {
        CLI::Option *flag = m_parser->add_flag(flag_name, description);
        if (flag)
        {
            flag->default_val(defaultVal);
        }
    }

    /**
     * @brief 获取指定标志的值。
     * @param flag_name 标志的名称。
     * @return 标志的值(true 或 false)。
     */
    bool getFlagVal(const std::string& flag_name)
    {
        return m_parser->get_option(flag_name)->as<bool>();
    }

    /**
     * @brief 解析命令行参数。
     * @param argc 命令行参数的数量。
     * @param argv 命令行参数的数组。
     * @return 如果解析成功,返回 0;如果解析失败,返回非 0 值。
     */
    auto parseArgs(int argc, char* argv[]) noexcept
    {
        try
        {
            m_parser->parse(argc, argv);
            return 0;
        }
        catch (const CLI::ParseError &e)
        {
            auto exit_code = e.get_exit_code();
            if(exit_code == CLI::CallForAllHelp().get_exit_code()){
                std::cout<< m_parser->help() << std::endl;
                std::exit(exit_code);
            } else {
                return exit_code;
            }
        }
    }

    /**
     * @brief 添加一个子命令。
     * @param name 子命令的名称。
     * @param description 子命令的描述。
     */
    void addSubCmd(const std::string& name, const std::string& description = "")
    {
        m_subcmdMap[name] = m_parser->add_subcommand(name, description);
    }

    /**
     * @brief 向指定的子命令添加一个选项。
     * @param subcmd 子命令的名称。
     * @param name 选项的名称。
     * @param defaultVal 选项的默认值。
     * @param description 选项的描述。
     */
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

    /**
     * @brief 获取指定子命令的选项值。
     * @tparam T 选项值的类型。
     * @param subcmd 子命令的名称。
     * @param option_name 选项的名称。
     * @param[out] ret 用于存储选项值的变量。
     */
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

     /**
     * @brief 获取子命令的字符串列表选项,使用分号作为分隔符
     * @param subcmd 子命令名称
     * @param option_name 选项名称
     * @param[out] ret 用于存储分割后的字符串列表
     */
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

    /**
     * @brief 获取字符串列表选项, 使用分号作为分隔符.
     * @param option_name 选项名称.
     * @param[out] ret 存储分割后的字符串列表.
     */
    void getOptionSplitStrList(const std::string& option_name, std::vector<std::string>& ret)
    {
        std::string ids_input;
        getOptionVal(option_name, ids_input);
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
    /**
     * @brief 向指定的子命令添加一个标志。
     * @param subcmd 子命令的名称。
     * @param flag_name 标志的名称。
     * @param defaultVal 标志的默认值。
     * @param description 标志的描述。
     */
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

    /**
     * @brief 获取指定子命令的标志值。
     * @param subcmd 子命令的名称。
     * @param flag_name 标志的名称。
     * @return 标志的值(true 或 false)。如果子命令不存在,返回 false。
     */
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
    /**
     * @brief 指向 CLI11 库的 CLI::App 对象的智能指针。这是主解析器。
     */
    std::unique_ptr<CLI::App> m_parser; // Change unique_ptr to shared_ptr for CLI::App
    /**
     * @brief 一个哈希表,用于存储子命令的名称和指向 CLI::App 对象的指针(子命令也是 CLI::App 对象)。
     */
    std::unordered_map<std::string, CLI::App*> m_subcmdMap;
};


} // namespace shared
} // namespace bsp_perf

#endif // __ARG_PARSER_HPP__
