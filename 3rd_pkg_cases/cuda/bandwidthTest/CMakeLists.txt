# bandwidthTest - CUDA 带宽测试（参考本目录 Makefile，不修改原 Makefile）
cmake_minimum_required(VERSION 3.18)

# 仅目标库模式（CUDA_PATH 无 nvcc）时不调用 find_package，直接用 CUDA_TARGET_LIB_DIR
if(NOT CUDA_TARGET_LIB_DIR)
  find_package(CUDAToolkit REQUIRED)
endif()

add_executable(bandwidthTest bandwidthTest.cu)

target_include_directories(bandwidthTest PRIVATE
  ${CUDA_SAMPLES_INCLUDE_DIR}
)
if(CUDA_TARGET_INCLUDE_DIR)
  target_include_directories(bandwidthTest PRIVATE ${CUDA_TARGET_INCLUDE_DIR})
endif()

if(CUDA_TARGET_LIB_DIR)
  target_link_directories(bandwidthTest PRIVATE ${CUDA_TARGET_LIB_DIR})
  target_link_libraries(bandwidthTest PRIVATE cudart)
  if(CUDA_TOOLCHAIN_RPATH_LINK)
    target_link_options(bandwidthTest PRIVATE ${CUDA_TOOLCHAIN_RPATH_LINK})
  endif()
else()
  target_link_libraries(bandwidthTest PRIVATE CUDA::cudart)
endif()

# CUDA 架构（与顶层 CMakeLists.txt 中 CUDA_ARCH_LIST 一致）
set_target_properties(bandwidthTest PROPERTIES
  CUDA_ARCHITECTURES "${CUDA_ARCH_LIST}"
  CUDA_STANDARD 11
)

if(CMAKE_CUDA_HOST_COMPILER)
  set_target_properties(bandwidthTest PROPERTIES
    CUDA_HOST_COMPILER "${CMAKE_CUDA_HOST_COMPILER}"
  )
endif()
