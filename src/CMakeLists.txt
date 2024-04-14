cmake_minimum_required(VERSION 3.12)

option(BUILD_SRC_FRAMEWORK "Build src/framework" ON)
option(BUILD_SRC_CORE "Build src/core" OFF)
option(BUILD_SRC_PROFILER "Build profiler" OFF)

# 根据选项包含子项目的CMakeLists.txt
if(BUILD_SRC_FRAMEWORK)
    add_subdirectory(framework)
endif()

if(BUILD_SRC_CORE)
    add_subdirectory(core)
endif()

if(BUILD_SRC_PROFILER)
    add_subdirectory(profiler)
endif()