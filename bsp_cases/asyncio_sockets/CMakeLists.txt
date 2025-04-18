cmake_minimum_required(VERSION 3.12)
project(asyncio_sockets VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

set(APP_NAME aio_tcpclient)
add_executable(${APP_NAME} aio_tcpclient.cpp)
target_link_libraries(${APP_NAME} PRIVATE  bsp_sockets bsp_shared)
target_include_directories(${APP_NAME} PRIVATE  ${CMAKE_SOURCE_DIR}/bsp)
set_target_properties(${APP_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")

install(TARGETS ${APP_NAME}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
)

set(APP_NAME aio_tcpserver)
add_executable(${APP_NAME} aio_tcpserver.cpp)
target_link_libraries(${APP_NAME} PRIVATE  bsp_sockets bsp_shared)
target_include_directories(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/bsp)
set_target_properties(${APP_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")

install(TARGETS ${APP_NAME}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
)

set(APP_NAME aio_udpclient)
add_executable(${APP_NAME} aio_udpclient.cpp)
target_link_libraries(${APP_NAME} PRIVATE  bsp_sockets bsp_shared)
target_include_directories(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/bsp)
set_target_properties(${APP_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")

install(TARGETS ${APP_NAME}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
)

set(APP_NAME aio_udpserver)
add_executable(${APP_NAME} aio_udpserver.cpp)
target_link_libraries(${APP_NAME} PRIVATE  bsp_sockets bsp_shared)
target_include_directories(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/bsp)
set_target_properties(${APP_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")

install(TARGETS ${APP_NAME}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
)

