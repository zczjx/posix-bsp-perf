if(BUILD_PLATFORM_JETSON)
    add_subdirectory(nvidia/TensorRT/resnet18)
endif()

if(BUILD_PLATFORM_RK35XX)
    add_subdirectory(rknn/yolo)
endif()