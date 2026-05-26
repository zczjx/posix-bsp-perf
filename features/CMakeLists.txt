cmake_minimum_required(VERSION 3.12)

option(BUILD_FEATURE_SVS "Build surround view feature" ON)

if(BUILD_FEATURE_SVS)
    add_subdirectory(svs)
endif()
