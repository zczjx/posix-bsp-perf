cmake_minimum_required(VERSION 3.12)

option(BUILD_PERF_CASE_COMMON "Build common cases" ON)

# 根据选项包含子项目的CMakeLists.txt
if(BUILD_PERF_CASE_COMMON)
    add_subdirectory(hello_world)
    add_subdirectory(ddr_bandwidth)
endif()

