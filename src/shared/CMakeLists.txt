# Set the minimum required version of CMake
cmake_minimum_required(VERSION 3.12)

# Set the project name
project(bsp_shared VERSION 0.0.1 LANGUAGES CXX)

# Set the language version
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SPDLOG REQUIRED spdlog)

include_directories(${SPDLOG_INCLUDE_DIRS})

# Add the source files
set(SOURCES
  BspLogger.hpp
  BspLogger.cpp
    # Add more source files here if needed
)

# Add the library target
add_library(${PROJECT_NAME} SHARED ${SOURCES})

message(STATUS "SPDLOG_LIBRARIES: ${SPDLOG_LIBRARIES}")
message(STATUS "SPDLOG_INCLUDE_DIRS: ${SPDLOG_INCLUDE_DIRS}")

if(SPDLOG_LIBRARIES)
    target_link_libraries(${PROJECT_NAME} PRIVATE spdlog pthread)
endif()

# Set the compile options
target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
    -rdynamic
    -g
    # Add more compile options here if needed
)

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

string(COMPARE EQUAL ${PROJECT_NAME} ${CMAKE_PROJECT_NAME} is_top_level)
if(is_top_level)
  message(FATAL_ERROR "This subproject must be built as part of the top-level project.")
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")

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

