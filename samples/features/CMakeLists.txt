cmake_minimum_required(VERSION 3.12)

if(BUILD_FEATURE_SVS)
    add_subdirectory(svs_view)
endif()

if(BUILD_FEATURE_FUSION)
    add_subdirectory(fusion_replay)
endif()
