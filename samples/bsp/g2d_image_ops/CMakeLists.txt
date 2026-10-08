cmake_minimum_required(VERSION 3.12)
project(g2d_image_ops VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

if(DEFINED BSP_LIB_PATH)
  set(OpenCV_DIR ${BSP_LIB_PATH}/cmake/opencv4)
else()
  set(OpenCV_DIR /usr/lib/cmake/opencv4)
endif()

find_package(OpenCV REQUIRED)

add_executable(${PROJECT_NAME}
  main.cpp
  OpenCVUtils.cpp
)

if(BUILD_PLATFORM_RK35XX)
  target_compile_definitions(${PROJECT_NAME} PRIVATE BUILD_PLATFORM_RK35XX)
endif()

if(BUILD_PLATFORM_JETSON)
  target_compile_definitions(${PROJECT_NAME} PRIVATE BUILD_PLATFORM_JETSON)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")

target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_SOURCE_DIR}/bsp
  ${OpenCV_INCLUDE_DIRS}
)

if(OpenCV_LIBRARIES)
  target_link_directories(${PROJECT_NAME} PRIVATE
    ${BSP_LIB_PATH}
    ${BSP_LIB_PATH}/nvidia
  )
  target_link_libraries(${PROJECT_NAME} PRIVATE ${OpenCV_LIBRARIES})
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE bsp_g2d bsp_image bsp_shared)

install(TARGETS ${PROJECT_NAME}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
)
