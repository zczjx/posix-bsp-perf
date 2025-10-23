cmake_minimum_required(VERSION 3.12)

project(Resnet18Plugin VERSION 0.0.1 LANGUAGES CXX)

# Set the C++ standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if(DEFINED BSP_PKG_CONFIG_PATH)
    # 设置环境变量 PKG_CONFIG_PATH
    set(ENV{PKG_CONFIG_PATH} ${BSP_PKG_CONFIG_PATH})
    message(STATUS "ENV{PKG_CONFIG_PATH}: $ENV{PKG_CONFIG_PATH}")
endif()

if(DEFINED BSP_LIB_PATH)
  set(OpenCV_DIR ${BSP_LIB_PATH}/cmake/opencv4)
else()
    set(OpenCV_DIR /usr/lib/cmake/opencv4)
endif()

# Find OpenCV package
find_package(OpenCV REQUIRED)

# Create the shared library for ResNet18 TrafficCamNet plugin
set(PLUGIN_NAME resnet18_trafficcamnet)

add_library(${PLUGIN_NAME} SHARED
    Resnet18TrafficCamNet.cpp
)

target_include_directories(${PLUGIN_NAME} PRIVATE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/bsp>
    $<INSTALL_INTERFACE:include>
    ${PC_MSGPACK_INCLUDE_DIRS}
)

if(OpenCV_LIBRARIES)
  target_link_options(${PLUGIN_NAME} PUBLIC "-Wl,-rpath,${OpenCV_LIBRARY_DIRS}" ${OpenCV_INCLUDE_LDFLAGS})
  target_include_directories(${PLUGIN_NAME} PUBLIC ${OpenCV_INCLUDE_DIRS})
  target_link_libraries(${PLUGIN_NAME} PRIVATE ${OpenCV_LIBRARIES})
endif()

set_target_properties(${PLUGIN_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")

install(TARGETS ${PLUGIN_NAME}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
)

# Optional: Install headers if needed
install(FILES Resnet18TrafficCamNet.hpp
  DESTINATION include/bsp_dnn/dnnObjDetector_plugins/resnet18
)

