#include <CLI/CLI.hpp>
#include <iostream>

int main(int argc, char **argv)
{
    CLI::App app{"App description"}; // 应用程序描述

    int p = 0;
    app.add_option("-p", p, "Parameter"); // 定义一个可选参数-p

    app.add_option("--case_name", "name of perf test case");
    app.add_option("--size_mb", "Memory size for the perf case");

    CLI11_PARSE(app, argc, argv); // 解析命令行参数

    std::cout << "Parameter value: " << p << std::endl; // 打印参数值

    return 0;
}