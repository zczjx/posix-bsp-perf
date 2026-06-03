cmake_minimum_required(VERSION 3.12)

option(BUILD_SAMPLES_BSP "Build BSP samples" ON)
option(BUILD_SAMPLES_MIDWARE "Build midware samples" ON)
option(BUILD_SAMPLES_FEATURES "Build feature samples" OFF)

if(BUILD_SAMPLES_BSP)
    add_subdirectory(bsp)
endif()

if(BUILD_SAMPLES_MIDWARE)
    add_subdirectory(midware)
endif()

if(BUILD_SAMPLES_FEATURES)
    add_subdirectory(features)
endif()
