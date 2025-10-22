# Set the minimum required version of CMake
cmake_minimum_required(VERSION 3.12)

# Set the project name
project(dnnObjDetector VERSION 0.0.1 LANGUAGES CXX)

# Set the language version
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig  REQUIRED)
pkg_check_modules(PC_MSGPACK REQUIRED msgpack)

# Add the source files
set(SOURCES
  dnnObjDetector.cpp
  dnnEngine_impl/IDnnEngine.cpp
)

# Add the library target
add_library(${PROJECT_NAME} SHARED ${SOURCES})

# target_link_libraries(${PROJECT_NAME} PRIVATE bsp_shared)

target_include_directories(${PROJECT_NAME} PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include>
    ${PC_MSGPACK_INCLUDE_DIRS}
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
target_link_libraries(${PROJECT_NAME} PRIVATE bsp_shared)

# Add platform-specific compile definitions and link libraries
if(BUILD_PLATFORM_RK35XX)
  target_compile_definitions(${PROJECT_NAME} PRIVATE BUILD_PLATFORM_RK35XX)
  target_link_libraries(${PROJECT_NAME} PUBLIC rknn_Engine)
  message(STATUS "Building dnnObjDetector for RK35XX platform")
endif()

if(BUILD_PLATFORM_JETSON)
  target_compile_definitions(${PROJECT_NAME} PRIVATE BUILD_PLATFORM_JETSON)
  target_link_libraries(${PROJECT_NAME} PUBLIC tensorrt_engine)
  message(STATUS "Building dnnObjDetector for Jetson platform")
endif()


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

add_subdirectory(dnnEngine_impl)
add_subdirectory(dnnObjDetector_plugins)

