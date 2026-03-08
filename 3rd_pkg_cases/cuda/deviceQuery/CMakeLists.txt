# deviceQuery - CUDA 设备查询（Runtime API，参考本目录 Makefile）
cmake_minimum_required(VERSION 3.18)

if(NOT CUDA_TARGET_LIB_DIR)
  find_package(CUDAToolkit REQUIRED)
endif()

add_executable(deviceQuery deviceQuery.cpp)

target_include_directories(deviceQuery PRIVATE
  ${CUDA_SAMPLES_INCLUDE_DIR}
)
if(CUDA_TARGET_INCLUDE_DIR)
  target_include_directories(deviceQuery PRIVATE ${CUDA_TARGET_INCLUDE_DIR})
endif()

if(CUDA_TARGET_LIB_DIR)
  target_link_directories(deviceQuery PRIVATE ${CUDA_TARGET_LIB_DIR})
  target_link_libraries(deviceQuery PRIVATE cudart)
  if(CUDA_TOOLCHAIN_RPATH_LINK)
    target_link_options(deviceQuery PRIVATE ${CUDA_TOOLCHAIN_RPATH_LINK})
  endif()
else()
  target_link_libraries(deviceQuery PRIVATE CUDA::cudart)
endif()

# 纯 C++/Runtime API，无 device 代码，无需 CUDA_ARCHITECTURES
target_compile_features(deviceQuery PRIVATE cxx_std_11)
