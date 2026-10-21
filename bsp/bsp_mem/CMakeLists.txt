cmake_minimum_required(VERSION 3.12)
project(bsp_mem VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

set(SOURCES
    ImageBufferPool.cpp
    impl/ImageBufferPoolBase.cpp
    impl/host/HostImageBufferPool.cpp
)

if(BUILD_PLATFORM_RK35XX)
    add_compile_definitions(BUILD_PLATFORM_RK35XX)
    list(APPEND SOURCES impl/rk_dmaheap/RkDmaHeapImageBufferPool.cpp)
endif()

if(BUILD_PLATFORM_JETSON)
    add_compile_definitions(BUILD_PLATFORM_JETSON)
    list(APPEND SOURCES impl/nv_surface/NvSurfaceImageBufferPool.cpp)
endif()

add_library(${PROJECT_NAME} SHARED ${SOURCES})

if(BUILD_PLATFORM_JETSON)
    find_library(NVBUFSURFACE_LIB nvbufsurface
        ${BSP_LIB_PATH}
        ${BSP_LIB_PATH}/nvidia
    )
    if(NVBUFSURFACE_LIB)
        message(STATUS "Found nvbufsurface: ${NVBUFSURFACE_LIB}")
        target_link_libraries(${PROJECT_NAME} PUBLIC ${NVBUFSURFACE_LIB})
    else()
        message(WARNING "nvbufsurface library not found, linking by name")
        target_link_libraries(${PROJECT_NAME} PUBLIC nvbufsurface)
    endif()

    # libnvbufsurface.so 依赖 libEGL.so.1 (位于 ${BSP_LIB_PATH})，交叉编译时 ld 需要
    # -rpath-link 才能解析间接依赖；PUBLIC 使下游可执行文件/库自动继承
    if(DEFINED BSP_LIB_PATH)
        target_link_options(${PROJECT_NAME} PUBLIC
            "-Wl,-rpath-link,${BSP_LIB_PATH}"
            "-Wl,-rpath-link,${BSP_LIB_PATH}/nvidia"
        )
    endif()

    target_include_directories(${PROJECT_NAME} PRIVATE
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/jetson_multimedia_api/include>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    )
endif()

target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(${PROJECT_NAME} PUBLIC bsp_image)

set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")

set(${PROJECT_NAME}_PC "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc")
configure_file(${CMAKE_SOURCE_DIR}/cmake/subProject.pc.in ${${PROJECT_NAME}_PC} @ONLY)
install(FILES ${${PROJECT_NAME}_PC} DESTINATION lib/pkgconfig)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include/${CMAKE_PROJECT_NAME}
    FILES_MATCHING
    PATTERN "*.h"
    PATTERN "*.hpp"
)
