cmake_minimum_required(VERSION 3.12)
project(zmq_pub_sub VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig  REQUIRED)
pkg_check_modules(PC_MSGPACK REQUIRED msgpack)

set(APP_NAME shmem_pub)
add_executable(${APP_NAME} shmem_pub.cpp)
target_link_libraries(${APP_NAME} PRIVATE  zeromq_ipc bsp_shared ${PC_MSGPACK_LDFLAGS})
target_include_directories(${APP_NAME} PRIVATE  ${CMAKE_SOURCE_DIR}/midware ${CMAKE_SOURCE_DIR}/bsp ${PC_MSGPACK_INCLUDE_DIRS})
set_target_properties(${APP_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")

install(TARGETS ${APP_NAME}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
)

set(APP_NAME shmem_sub)
add_executable(${APP_NAME} shmem_sub.cpp)
target_link_libraries(${APP_NAME} PRIVATE  zeromq_ipc bsp_shared ${PC_MSGPACK_LDFLAGS})
target_include_directories(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/midware ${CMAKE_SOURCE_DIR}/bsp ${PC_MSGPACK_INCLUDE_DIRS})
set_target_properties(${APP_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")

install(TARGETS ${APP_NAME}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
)

