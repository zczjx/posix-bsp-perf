cmake_minimum_required(VERSION 3.12)
project(obj_detector VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig  REQUIRED)
pkg_check_modules(PC_MSGPACK REQUIRED msgpack)

add_executable(${PROJECT_NAME}
    main.cpp
    objDetector.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/data_recorder
    ${CMAKE_SOURCE_DIR}/midware
    ${CMAKE_SOURCE_DIR}/bsp
    ${CMAKE_SOURCE_DIR}/3rdparty
    ${PC_MSGPACK_INCLUDE_DIRS}
)
target_link_libraries(${PROJECT_NAME} PRIVATE
  zeromq_ipc
  bsp_shared
  dnnObjDetector
  ${PC_MSGPACK_LDFLAGS}
)

set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")

install(TARGETS ${PROJECT_NAME}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
)