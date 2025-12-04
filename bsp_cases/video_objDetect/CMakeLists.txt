cmake_minimum_required(VERSION 3.12)
project(video_objDetect VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

if(DEFINED BSP_LIB_PATH)
  set(OpenCV_DIR ${BSP_LIB_PATH}/cmake/opencv4)
else()
    set(OpenCV_DIR /usr/lib/cmake/opencv4)
endif()
# Find OpenCV package
find_package(OpenCV REQUIRED)

add_executable(${PROJECT_NAME} main.cpp)

if(BUILD_PLATFORM_RK35XX)
  target_compile_definitions(${PROJECT_NAME} PRIVATE BUILD_PLATFORM_RK35XX)
endif()

if(BUILD_PLATFORM_JETSON)
  target_compile_definitions(${PROJECT_NAME} PRIVATE BUILD_PLATFORM_JETSON)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")

target_include_directories(${PROJECT_NAME} PUBLIC ${OpenCV_INCLUDE_DIRS})

if(OpenCV_LIBRARIES)
  target_link_directories(${PROJECT_NAME} PRIVATE ${OpenCV_LIBRARY_DIRS})
  target_link_libraries(${PROJECT_NAME} PRIVATE ${OpenCV_LIBRARIES})
  target_include_directories(${PROJECT_NAME} PUBLIC ${OpenCV_INCLUDE_DIRS})
endif()

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/bsp)
target_link_libraries(${PROJECT_NAME} PRIVATE bsp_g2d bsp_enc bsp_dec dnnObjDetector case_framework bsp_shared)

install(TARGETS ${PROJECT_NAME}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
)
