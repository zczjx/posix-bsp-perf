cmake_minimum_required(VERSION 3.12)

option(BUILD_MIDWARE_ZMQ_IPC "Build zeromq ipc" ON)

if(BUILD_MIDWARE_ZMQ_IPC)
    add_subdirectory(zeromq_ipc)
endif()
