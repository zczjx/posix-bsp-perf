cmake_minimum_required(VERSION 3.12)
project(svs_view VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

if(DEFINED BSP_LIB_PATH)
  set(OpenCV_DIR ${BSP_LIB_PATH}/cmake/opencv4)
else()
    set(OpenCV_DIR /usr/lib/cmake/opencv4)
endif()
# Find OpenCV package
find_package(OpenCV REQUIRED)

add_executable(${PROJECT_NAME}
    SvsViewApp.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/features
    ${CMAKE_SOURCE_DIR}/bsp
    ${OpenCV_INCLUDE_DIRS}
)

if(OpenCV_LIBRARIES)
    target_link_directories(${PROJECT_NAME} PRIVATE ${OpenCV_LIBRARY_DIRS})
    target_link_libraries(${PROJECT_NAME} PRIVATE
        feature_svs
        ${OpenCV_LIBRARIES}
    )
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)
