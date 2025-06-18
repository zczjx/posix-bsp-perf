cmake_minimum_required(VERSION 3.12)

option(BUILD_MIDWARE_CASE_COMMON "Build Midware common cases" ON)

if(BUILD_MIDWARE_CASE_COMMON)
    add_subdirectory(zmq_pub_sub)
    add_subdirectory(zmq_shmem)
endif()


