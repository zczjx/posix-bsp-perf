cmake_minimum_required(VERSION 3.12)
project(zmq_pub_sub VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

set(APP_NAME zmq_pub)
add_executable(${APP_NAME} pub_main.cpp)
target_link_libraries(${APP_NAME} PRIVATE  zeromq_ipc bsp_shared)
target_include_directories(${APP_NAME} PRIVATE  ${CMAKE_SOURCE_DIR}/midware ${CMAKE_SOURCE_DIR}/bsp)
set_target_properties(${APP_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")

install(TARGETS ${APP_NAME}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
)

set(APP_NAME zmq_sub)
add_executable(${APP_NAME} sub_main.cpp)
target_link_libraries(${APP_NAME} PRIVATE  zeromq_ipc bsp_shared)
target_include_directories(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/midware ${CMAKE_SOURCE_DIR}/bsp)
set_target_properties(${APP_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")

install(TARGETS ${APP_NAME}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
)

