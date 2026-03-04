# Set the minimum required version of CMake
cmake_minimum_required(VERSION 3.12)

project(tensorrt_engine VERSION 0.0.1 LANGUAGES CXX)

# Set the language version
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if(DEFINED BSP_CUDA_PATH)
    set(CMAKE_CUDA_COMPILER "${BSP_CUDA_PATH}/bin/nvcc")
    set(CUDA_TOOLKIT_ROOT_DIR ${BSP_CUDA_PATH})
    set(CUDAToolkit_ROOT ${BSP_CUDA_PATH})
endif()

# Avoid FindCUDA warning "Expecting to find librt for libcudart_static":
# use dynamic cudart (libcudart.so) instead of static, so librt is not required.
set(CUDA_USE_STATIC_CUDA_RUNTIME OFF CACHE BOOL "Use static CUDA runtime" FORCE)
# Pre-find librt so FindCUDA can use it if needed (cross-compile: set CMAKE_FIND_ROOT_PATH for target sysroot).
find_library(CUDA_rt_LIBRARY rt)
find_package(CUDA REQUIRED)

find_path(TensorRT_INCLUDE_DIR NvInfer.h
    PATHS ${BSP_INCLUDE_PATH}
    NO_DEFAULT_PATH
)

find_library(TensorRT_LIBRARY nvinfer
    PATHS ${BSP_LIB_PATH}
    NO_DEFAULT_PATH
)


find_library(TensorRT_ONNX_PARSER nvonnxparser
    PATHS ${BSP_LIB_PATH}
    NO_DEFAULT_PATH)

message(STATUS "TensorRT include dir: ${TensorRT_INCLUDE_DIR}")
message(STATUS "TensorRT library: ${TensorRT_LIBRARY}")
message(STATUS "TensorRT ONNX parser: ${TensorRT_ONNX_PARSER}")

set(SOURCES
    tensorrt.cpp
)

# Add the library target
add_library(${PROJECT_NAME} SHARED ${SOURCES})
message(STATUS "CUDA_INCLUDE_DIRS: ${CUDA_INCLUDE_DIRS}")

target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/bsp>
    $<INSTALL_INTERFACE:include>
    ${CUDA_INCLUDE_DIRS}
    ${TensorRT_INCLUDE_DIR}
)

# 指定pkgconfig文件的内容
set(${PROJECT_NAME}_PC "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc")

# 配置pkgconfig文件
configure_file(${CMAKE_SOURCE_DIR}/cmake/subProject.pc.in ${${PROJECT_NAME}_PC} @ONLY)

# 安装pkgconfig文件
install(FILES ${${PROJECT_NAME}_PC} DESTINATION lib/pkgconfig)

string(COMPARE EQUAL ${PROJECT_NAME} ${CMAKE_PROJECT_NAME} is_top_level)
if(is_top_level)
  message(FATAL_ERROR "This subproject must be built as part of the top-level project.")
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")
target_link_directories(${PROJECT_NAME} PUBLIC
    ${BSP_LIB_PATH}
    ${BSP_LIB_PATH}/nvidia
)
target_link_libraries(${PROJECT_NAME} PUBLIC
    ${CUDA_LIBRARIES}
    ${TensorRT_LIBRARY}
    ${TensorRT_ONNX_PARSER}
    bsp_shared
)

# Specify the installation directories
install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION lib
)

