# 设置CMake最小版本
cmake_minimum_required(VERSION 3.12)

# 设置项目名称
project(PosixPerf)

# Set the language version
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)

set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/install")


# 添加子目录
add_subdirectory(src)
add_subdirectory(perf_cases)


# Clean target
add_custom_target(clean-all
    COMMAND ${CMAKE_BUILD_TOOL} clean
    COMMAND rm -rf ${CMAKE_INSTALL_PREFIX}
)
