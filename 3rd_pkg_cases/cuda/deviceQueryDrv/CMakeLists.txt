# deviceQueryDrv - CUDA 设备查询（Driver API，参考本目录 Makefile）
# 使用 libcuda（Driver API）；交叉编译时链接 targets/<arch>-linux/lib/stubs 中的 stub
cmake_minimum_required(VERSION 3.18)

# 仅目标库模式不依赖 FindCUDAToolkit，直接用 stubs
if(NOT CUDA_TARGET_LIB_DIR)
  find_package(CUDAToolkit REQUIRED)
endif()

add_executable(deviceQueryDrv deviceQueryDrv.cpp)

target_include_directories(deviceQueryDrv PRIVATE
  ${CUDA_SAMPLES_INCLUDE_DIR}
)
if(CUDA_TARGET_INCLUDE_DIR)
  target_include_directories(deviceQueryDrv PRIVATE ${CUDA_TARGET_INCLUDE_DIR})
endif()

# Driver API：链接 libcuda。交叉编译时需使用 CUDA 提供的 stubs
# CUDA_PATH 可能为 target-only（include + lib，无 nvcc），此时 lib 即 targets/aarch64-linux/lib，stubs 在 lib/stubs
if(CROSS_TARGET_ARCH STREQUAL "aarch64" AND CUDA_PATH)
  if(EXISTS "${CUDA_PATH}/lib/stubs/libcuda.so")
    set(CUDA_DRV_STUBS "${CUDA_PATH}/lib/stubs")
  else()
    set(CUDA_DRV_STUBS "${CUDA_PATH}/targets/aarch64-linux/lib/stubs")
  endif()
  if(EXISTS "${CUDA_DRV_STUBS}/libcuda.so")
    target_link_directories(deviceQueryDrv PRIVATE ${CUDA_DRV_STUBS})
    target_link_libraries(deviceQueryDrv PRIVATE cuda)
  else()
    target_link_libraries(deviceQueryDrv PRIVATE CUDA::cuda_driver)
  endif()
elseif(CROSS_TARGET_ARCH STREQUAL "x86_64" AND CUDA_PATH)
  set(CUDA_DRV_STUBS "${CUDA_PATH}/targets/x86_64-linux/lib/stubs")
  if(EXISTS "${CUDA_DRV_STUBS}/libcuda.so")
    target_link_directories(deviceQueryDrv PRIVATE ${CUDA_DRV_STUBS})
    target_link_libraries(deviceQueryDrv PRIVATE cuda)
  else()
    target_link_libraries(deviceQueryDrv PRIVATE CUDA::cuda_driver)
  endif()
else()
  target_link_libraries(deviceQueryDrv PRIVATE CUDA::cuda_driver)
endif()

# 纯 C++/Driver API，无 device 代码
target_compile_features(deviceQueryDrv PRIVATE cxx_std_11)
