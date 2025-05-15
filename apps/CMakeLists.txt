cmake_minimum_required(VERSION 3.12)

option(BUILD_APP_DATA_RECORDER "Build App data recorder" OFF)

if(BUILD_APP_DATA_RECORDER)
    add_subdirectory(data_recorder)
endif()


