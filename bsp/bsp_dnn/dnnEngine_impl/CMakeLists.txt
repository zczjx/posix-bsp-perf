if(ENABLE_RKNN)
    add_subdirectory(rknn)
endif()

if(ENABLE_TENSORRT)
    add_subdirectory(tensorRT)
endif()