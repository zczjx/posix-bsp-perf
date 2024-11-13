cmake_minimum_required(VERSION 3.12)

project(YOLOPlugin VERSION 0.0.1 LANGUAGES CXX)

# Set the C++ standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if(DEFINED BSP_PKG_CONFIG_PATH)
    # 设置环境变量 PKG_CONFIG_PATH
    set(ENV{PKG_CONFIG_PATH} ${BSP_PKG_CONFIG_PATH})
    message(STATUS "ENV{PKG_CONFIG_PATH}: $ENV{PKG_CONFIG_PATH}")
endif()
# Find OpenCV package
find_package(PkgConfig REQUIRED)
pkg_check_modules(PC_OPENCV REQUIRED OpenCV)

# Add all cpp files in the current directory to the project

# Create the shared library
set(PLUGIN_NAME rknn_yolov5)

add_library(${PLUGIN_NAME} SHARED
    rknnYolov5.cpp
    common/YoloPostProcess.cpp
)

if(PC_OPENCV_LIBRARIES)
  target_link_options(${PLUGIN_NAME} PRIVATE ${PC_OPENCV_LDFLAGS} "-Wl,-rpath,${PC_OPENCV_LIBRARY_DIRS}")
  target_include_directories(${PROJECT_NAME} PUBLIC ${PC_OPENCV_INCLUDE_DIRS})
endif()
set_target_properties(${PLUGIN_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")

install(TARGETS ${PLUGIN_NAME}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
)