cmake_minimum_required(VERSION 3.12)

project(zeromq_ipc VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)


find_package(PkgConfig  REQUIRED)
pkg_check_modules(PC_ZMQ REQUIRED libzmq)

set(SOURCES
  zmqPublisher.cpp
  zmqSubscriber.cpp
)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")

target_link_libraries(${PROJECT_NAME} PRIVATE ${PC_ZMQ_LDFLAGS})

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



