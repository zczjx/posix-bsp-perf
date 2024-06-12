# Set the minimum required version of CMake
cmake_minimum_required(VERSION 3.12)

# Set the project name
project(bsp_shared VERSION 0.0.1 LANGUAGES CXX)

# Set the language version
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)


message(STATUS "CMAKE_MODULE_PATH: ${CMAKE_MODULE_PATH}")

# 检查是否已设置 MY_PKG_CONFIG_PATH 变量
if(DEFINED BSP_PKG_CONFIG_PATH)
    # 设置环境变量 PKG_CONFIG_PATH
    set(ENV{PKG_CONFIG_PATH} ${BSP_PKG_CONFIG_PATH})
    message(STATUS "ENV{PKG_CONFIG_PATH}: $ENV{PKG_CONFIG_PATH}")
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(PC_SPDLOG REQUIRED spdlog)
pkg_check_modules(PC_CLI11 REQUIRED CLI11)



message(STATUS "PC_SPDLOG_LIBRARY_DIRS: ${PC_SPDLOG_LIBRARY_DIRS}")
message(STATUS "PC_SPDLOG_INCLUDE_DIRS: ${PC_SPDLOG_INCLUDE_DIRS}")
message(STATUS "PC_SPDLOG_LDFLAGS: ${PC_SPDLOG_LDFLAGS}")
message(STATUS "PC_SPDLOG_CFLAGS: ${PC_SPDLOG_CFLAGS}")
message(STATUS "PC_SPDLOG_LIBRARIES: ${PC_SPDLOG_LIBRARIES}")

message(STATUS "PC_CLI11_LIBRARY_DIRS: ${PC_CLI11_LIBRARY_DIRS}")
message(STATUS "PC_CLI11_INCLUDE_DIRS: ${PC_CLI11_INCLUDE_DIRS}")
message(STATUS "PC_CLI11_LDFLAGS: ${PC_CLI11_LDFLAGS}")
message(STATUS "PC_CLI11_CFLAGS: ${PC_CLI11_CFLAGS}")
message(STATUS "PC_CLI11_LIBRARIES: ${PC_CLI11_LIBRARIES}")

# Add the source files
set(SOURCES
  ArgParser.hpp
  BspLogger.hpp
  BspLogger.cpp
    # Add more source files here if needed
)

# Add the library target
add_library(${PROJECT_NAME} SHARED ${SOURCES})

if(PC_SPDLOG_LIBRARIES)
  target_link_options(${PROJECT_NAME} PRIVATE ${PC_SPDLOG_LDFLAGS} "-Wl,-rpath,${PC_SPDLOG_LIBRARY_DIRS}")
  # target_link_libraries(${PROJECT_NAME} PUBLIC ${PC_SPDLOG_LIBRARIES})
  target_include_directories(${PROJECT_NAME} PUBLIC ${PC_SPDLOG_INCLUDE_DIRS})
endif()

if(PC_CLI11_CFLAGS)
  target_include_directories(${PROJECT_NAME} PUBLIC ${PC_CLI11_INCLUDE_DIRS})
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

