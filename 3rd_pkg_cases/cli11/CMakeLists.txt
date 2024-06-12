# Set the minimum required version of CMake
cmake_minimum_required(VERSION 3.12)

# Set the project name
project(cli11_case VERSION 0.0.1 LANGUAGES CXX)

# Set the language version
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/install")


message(STATUS "CMAKE_MODULE_PATH: ${CMAKE_MODULE_PATH}")

# 检查是否已设置 MY_PKG_CONFIG_PATH 变量
if(DEFINED BSP_PKG_CONFIG_PATH)
    # 设置环境变量 PKG_CONFIG_PATH
    set(ENV{PKG_CONFIG_PATH} ${BSP_PKG_CONFIG_PATH})
    message(STATUS "ENV{PKG_CONFIG_PATH}: $ENV{PKG_CONFIG_PATH}")
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(PC_CLI11 REQUIRED CLI11)


# Add the source files
set(SOURCES
  main.cpp
    # Add more source files here if needed
)


add_executable(${PROJECT_NAME} ${SOURCES})

if(PC_CLI11_CFLAGS)
  message(STATUS "PC_CLI11_CFLAGS: ${PC_CLI11_CFLAGS}")
  message(STATUS "PC_CLI11_INCLUDE_DIRS: ${PC_CLI11_INCLUDE_DIRS}")
  target_include_directories(${PROJECT_NAME} PUBLIC ${PC_CLI11_INCLUDE_DIRS})
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include>
)

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

