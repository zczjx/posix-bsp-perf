# Set the minimum required version of CMake
cmake_minimum_required(VERSION 3.12)

# Set the project name
project(bsp_container VERSION 0.0.1 LANGUAGES CXX)

# Set the language version
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)


if(DEFINED BSP_PKG_CONFIG_PATH)
    # 设置环境变量 PKG_CONFIG_PATH
    set(ENV{PKG_CONFIG_PATH} ${BSP_PKG_CONFIG_PATH})
    message(STATUS "ENV{PKG_CONFIG_PATH}: $ENV{PKG_CONFIG_PATH}")
endif()

find_package(PkgConfig  REQUIRED)
# Find ffmpeg libraries
pkg_check_modules(PC_AVFORMAT REQUIRED libavformat)
pkg_check_modules(PC_AVUTIL REQUIRED libavutil)

# Add the source files
set(SOURCES
  impl/IDemuxer.cpp
  impl/IMuxer.cpp
  impl/ffmpeg/FFmpegDemuxer.cpp
  impl/ffmpeg/FFmpegMuxer.cpp
)

# Add the library target
add_library(${PROJECT_NAME} SHARED ${SOURCES})

set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")
target_link_libraries(${PROJECT_NAME} PRIVATE bsp_shared)

if(PC_AVFORMAT_LIBRARIES)
  target_link_libraries(${PROJECT_NAME} PRIVATE ${PC_AVFORMAT_LDFLAGS})
  target_include_directories(${PROJECT_NAME} PRIVATE ${PC_AVFORMAT_INCLUDE_DIRS})
endif()

if(PC_AVUTIL_LIBRARIES)
  target_link_libraries(${PROJECT_NAME} PRIVATE ${PC_AVUTIL_LDFLAGS})
  target_include_directories(${PROJECT_NAME} PRIVATE ${PC_AVUTIL_INCLUDE_DIRS})
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include>
)

# 指定pkgconfig文件的内容
set(${PROJECT_NAME}_PC "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc")

# 配置pkgconfig文件
configure_file(${CMAKE_SOURCE_DIR}/cmake/subProject.pc.in ${${PROJECT_NAME}_PC} @ONLY)

# 安装pkgconfig文件
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

