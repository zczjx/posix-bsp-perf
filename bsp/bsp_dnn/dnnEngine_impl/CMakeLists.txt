if(BUILD_PLATFORM_RK35XX)
    add_subdirectory(rknn)
endif()

if(BUILD_PLATFORM_JETSON)
    add_subdirectory(tensorrt)
endif()