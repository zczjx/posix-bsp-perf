cmake_minimum_required(VERSION 3.12)

option(BUILD_MIDWARE_SAMPLE_COMMON "Build common midware samples" ON)

if(BUILD_MIDWARE_SAMPLE_COMMON)
    add_subdirectory(zmq_pub_sub)
    add_subdirectory(zmq_shmem)
endif()


