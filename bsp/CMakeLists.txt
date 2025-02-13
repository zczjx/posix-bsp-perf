cmake_minimum_required(VERSION 3.12)

option(BUILD_SRC_FRAMEWORK "Build bsp/framework" ON)
option(BUILD_SRC_SHARED "Build bsp/shared" ON)
option(BUILD_SRC_CORE "Build bsp/core" OFF)
option(BUILD_SRC_PROFILER "Build profiler" ON)
option(BUILD_SRC_BSP_SOCKETS "Build bsp_sockets" ON)
option(BUILD_SRC_BSP_CONTAINER "Build bsp_container" ON)

if(BUILD_PLATFORM_RK35XX)
    option(BUILD_SRC_BSP_DNN "Build bsp_dnn" ON)
    option(BUILD_SRC_BSP_G2D "Build bsp_g2d" ON)
    option(BUILD_SRC_BSP_CODEC "Build bsp_codec" ON)
else()
    option(BUILD_SRC_BSP_DNN "Build bsp_dnn" OFF)
    option(BUILD_SRC_BSP_G2D "Build bsp_g2d" OFF)
    option(BUILD_SRC_BSP_CODEC "Build bsp_codec" OFF)
endif()

# 根据选项包含子项目的CMakeLists.txt
if(BUILD_SRC_FRAMEWORK)
    add_subdirectory(framework)
endif()

if(BUILD_SRC_SHARED)
    add_subdirectory(shared)
endif()

if(BUILD_SRC_CORE)
    add_subdirectory(core)
endif()

if(BUILD_SRC_PROFILER)
    add_subdirectory(profiler)
endif()

if(BUILD_SRC_BSP_SOCKETS)
    add_subdirectory(bsp_sockets)
endif()

if(BUILD_SRC_BSP_DNN)
    add_subdirectory(bsp_dnn)
endif()

if(BUILD_SRC_BSP_G2D)
    add_subdirectory(bsp_g2d)
endif()

if(BUILD_SRC_BSP_CODEC)
    add_subdirectory(bsp_codec)
endif()

if(BUILD_SRC_BSP_CONTAINER)
    add_subdirectory(bsp_container)
endif()