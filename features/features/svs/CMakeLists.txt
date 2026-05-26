cmake_minimum_required(VERSION 3.12)
project(feature_svs VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

if(DEFINED BSP_LIB_PATH)
    set(OpenCV_DIR ${BSP_LIB_PATH}/cmake/opencv4)
else()
    if(BUILD_PLATFORM_RK35XX)
        set(OpenCV_DIR /usr/lib/cmake/opencv4)
    endif()
endif()

find_package(OpenCV REQUIRED)

set(SOURCES
    ISurroundView.cpp
    SvsCalibrator.cpp
    SvsParamLoader.cpp
    SvsProjector.cpp
    SvsBlender.cpp
    impl/opencv/OpenCvSurroundView.cpp
)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<INSTALL_INTERFACE:include>
        ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        ${OpenCV_LIBS}
)

set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include/svs
    FILES_MATCHING
    PATTERN "*.hpp"
)
