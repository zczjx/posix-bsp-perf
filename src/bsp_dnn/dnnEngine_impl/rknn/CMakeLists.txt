# Set the minimum required version of CMake
cmake_minimum_required(VERSION 3.12)

project(rknn_Engine VERSION 0.0.1 LANGUAGES CXX)

# Set the language version
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if(DEFINED BSP_LIB_PATH)
    set(RKNN_RT_LIB ${BSP_LIB_PATH}/librknnrt.so)
else()
    set(RKNN_RT_LIB /usr/lib/librknnrt.so)
endif()

set(SOURCES
    rknn.cpp
)

# Add the library target
add_library(${PROJECT_NAME} SHARED ${SOURCES})


target_include_directories(${PROJECT_NAME} PRIVATE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

# 指定pkgconfig文件的内容
set(${PROJECT_NAME}_PC "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc")

# 配置pkgconfig文件
configure_file(${CMAKE_SOURCE_DIR}/cmake/subProject.pc.in ${${PROJECT_NAME}_PC} @ONLY)

# 安装pkgconfig文件
install(FILES ${${PROJECT_NAME}_PC} DESTINATION lib/pkgconfig)

string(COMPARE EQUAL ${PROJECT_NAME} ${CMAKE_PROJECT_NAME} is_top_level)
if(is_top_level)
  message(FATAL_ERROR "This subproject must be built as part of the top-level project.")
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")
target_link_libraries(${PROJECT_NAME} PUBLIC ${RKNN_RT_LIB} bsp_shared)
# Specify the installation directories
install(TARGETS rknn_Engine
    LIBRARY DESTINATION lib
)