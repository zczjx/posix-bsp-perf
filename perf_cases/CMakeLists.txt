cmake_minimum_required(VERSION 3.12)

option(BUILD_PERF_CASE_HELLO_WORLD "Build perf_cases/hello_world" ON)

# 根据选项包含子项目的CMakeLists.txt
if(BUILD_PERF_CASE_HELLO_WORLD)
    add_subdirectory(hello_world)
endif()

