cmake_minimum_required(VERSION 3.12)
project(data_recorder VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

add_executable(${PROJECT_NAME}
    main.cpp
    CarlaVehicle.cpp
    sensor_clients/SensorManager.cpp
    sensor_clients/SensorClient.cpp
    sensor_clients/camera/CameraClient.cpp
    sensor_clients/camera/VideoDecHelper.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/apps/data_recorder ${CMAKE_SOURCE_DIR}/midware ${CMAKE_SOURCE_DIR}/bsp ${CMAKE_SOURCE_DIR}/3rdparty)
target_link_libraries(${PROJECT_NAME} PRIVATE  zeromq_ipc bsp_shared bsp_protocol bsp_dec)
set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")

install(TARGETS ${PROJECT_NAME}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
)