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
pkg_check_modules(PC_FFMPEG REQUIRED libavformat libavutil libavcodec libswresample)
# Add the source files
set(SOURCES
  impl/IDemuxer.cpp
  impl/IMuxer.cpp
  impl/ffmpeg/FFmpegDemuxer.cpp
  impl/ffmpeg/FFmpegStreamWriter.cpp
  impl/ffmpeg/FFmpegMuxer.cpp
)

# Add the library target
add_library(${PROJECT_NAME} SHARED ${SOURCES})

set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")
target_link_libraries(${PROJECT_NAME} PRIVATE bsp_shared)

if(PC_FFMPEG_LIBRARIES)
  if(ENABLE_RK_MPP)
    if(DEFINED BSP_LIB_PATH)
      set(RK_MPP_LIB ${BSP_LIB_PATH}/librockchip_mpp.so)
    else()
      set(RK_MPP_LIB /usr/lib/librockchip_mpp.so)
    endif()
    target_link_libraries(${PROJECT_NAME} PUBLIC ${RK_MPP_LIB})
  endif()
  if(ENABLE_RKRGA)
    if(DEFINED BSP_LIB_PATH)
      set(RKRGA_LIB ${BSP_LIB_PATH}/librga.so)
    else()
      set(RKRGA_LIB /usr/lib/librga.so)
    endif()
    target_link_libraries(${PROJECT_NAME} PUBLIC ${RKRGA_LIB})
  endif()
  message(STATUS "PC_FFMPEG_LDFLAGS: ${PC_FFMPEG_LDFLAGS}")
  message(STATUS "PC_FFMPEG_LIBRARY_DIRS: ${PC_FFMPEG_LIBRARY_DIRS}")
  target_link_libraries(${PROJECT_NAME} PUBLIC ${PC_FFMPEG_LDFLAGS})
  target_include_directories(${PROJECT_NAME} PUBLIC ${PC_FFMPEG_INCLUDE_DIRS})
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

