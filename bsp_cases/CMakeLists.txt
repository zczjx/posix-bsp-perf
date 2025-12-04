cmake_minimum_required(VERSION 3.12)

option(BUILD_PERF_CASE_COMMON "Build common cases" ON)

if(BUILD_PERF_CASE_COMMON)
    add_subdirectory(hello_world)
    add_subdirectory(json_parse)
    add_subdirectory(ddr_bandwidth)
    add_subdirectory(asyncio_sockets)
    add_subdirectory(demux_mux)
endif()

if(BUILD_PLATFORM_RK35XX)
    add_subdirectory(dnn_objDetect)
    add_subdirectory(video_objDetect)
    add_subdirectory(video_dec_enc)
endif()

if(BUILD_PLATFORM_JETSON)
    add_subdirectory(dnn_objDetect)
    add_subdirectory(video_objDetect)
    add_subdirectory(video_dec_enc)
    add_subdirectory(egl_render)
endif()

