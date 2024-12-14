# Set the minimum required version of CMake
cmake_minimum_required(VERSION 3.12)

# Set the project name
project(bsp_sockets VERSION 0.0.2 LANGUAGES CXX)

# Set the language version
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Add the source files
set(SOURCES
  impl/IEventLoop.cpp
  impl/EventLoopEpoll.cpp
  impl/EventLoopPoll.cpp
  impl/EventLoopLibevent.cpp
  impl/IOBuffer.cpp
  impl/TcpClient.cpp
  impl/TcpConnection.cpp
  impl/TcpServer.cpp
  impl/ThreadPool.cpp
  impl/TimerQueue.cpp
  impl/UdpClient.cpp
  impl/UdpServer.cpp
    # Add more source files here if needed
)

# Add the library target
add_library(${PROJECT_NAME} SHARED ${SOURCES})

# Set the path to the cross-compiled libevent
#set(CMAKE_PREFIX_PATH "/home/builder/temp/aarch64")


# Manually set LIBEVENT_LIBRARIES and LIBEVENT_LIBRARY_DIRS
#set(LIBEVENT_LIBRARIES "/home/builder/temp/aarch64/lib/libevent.so")
#set(LIBEVENT_LIBRARY_DIRS "/home/builder/temp/aarch64/lib")
#set(LIBEVENT_INCLUDE_DIRS "/home/builder/temp/aarch64/include")

# 检查是否已设置 MY_PKG_CONFIG_PATH 变量
if(DEFINED BSP_PKG_CONFIG_PATH)
    # 设置环境变量 PKG_CONFIG_PATH
    set(ENV{PKG_CONFIG_PATH} ${BSP_PKG_CONFIG_PATH})
    message(STATUS "ENV{PKG_CONFIG_PATH}: $ENV{PKG_CONFIG_PATH}")
endif()

# Find the libevent package
find_package(PkgConfig REQUIRED)
pkg_check_modules(PC_LIBEVENT REQUIRED libevent)


if(PC_LIBEVENT_LIBRARIES)
  target_link_options(${PROJECT_NAME} PUBLIC ${PC_LIBEVENT_LDFLAGS} "-Wl,-rpath,${PC_LIBEVENT_LIBRARY_DIRS}")
  # target_link_libraries(${PROJECT_NAME} PUBLIC ${PC_LIBEVENT_LIBRARIES})
  target_include_directories(${PROJECT_NAME} PUBLIC ${PC_LIBEVENT_INCLUDE_DIRS})
endif()

# target_link_libraries(${PROJECT_NAME} PRIVATE bsp_shared)

target_include_directories(${PROJECT_NAME} PUBLIC
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
target_link_libraries(${PROJECT_NAME} PUBLIC bsp_shared)

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

