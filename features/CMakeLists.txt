cmake_minimum_required(VERSION 3.12)

option(BUILD_FEATURE_SVS "Build surround view feature" ON)
option(BUILD_FEATURE_FUSION "Build GNSS+IMU EKF fusion feature" ON)

if(BUILD_FEATURE_SVS)
    add_subdirectory(svs)
endif()

if(BUILD_FEATURE_FUSION)
    add_subdirectory(fusion)
endif()
