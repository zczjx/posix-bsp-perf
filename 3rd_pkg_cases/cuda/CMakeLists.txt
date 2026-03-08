# CUDA samples - CMake 构建（参考子目录 Makefile，不修改原 Makefile）
#
# 本机编译:
#   cmake -S . -B build [-DCUDA_PATH=/path/to/cuda]
#   cmake --build build
#
# aarch64 交叉编译（推荐使用 toolchain）:
#   cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-linux-gnu.cmake [-DCUDA_PATH=...] [-DTARGET_FS=/path/to/rootfs]
#   cmake --build build
#
# 或仅指定目标架构（需自行指定 C/CXX 编译器）:
#   cmake -S . -B build -DCROSS_TARGET_ARCH=aarch64 -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++
#
cmake_minimum_required(VERSION 3.18)

# -----------------------------------------------------------------------------
# CUDA 路径必须在 project() 之前确定
# 支持两种用法：
#   - CUDA_PATH 含 nvcc：用该路径的 nvcc 与库（与之前行为一致）
#   - CUDA_PATH 仅含目标库（如 aarch64 的 include/lib，无 bin/nvcc）：用 host 的 nvcc + 该路径的头与库
# -----------------------------------------------------------------------------
if(DEFINED ENV{CUDA_PATH})
  set(CUDA_PATH "$ENV{CUDA_PATH}" CACHE PATH "CUDA Toolkit root (or target-only include/lib)")
endif()
if(NOT CUDA_PATH)
  set(CUDA_PATH "/usr/local/cuda" CACHE PATH "CUDA Toolkit root")
endif()

# 仅当 CUDA_PATH 下存在 nvcc 时才指定为 CUDA 编译器；否则用 host nvcc + 目标头/库
if(EXISTS "${CUDA_PATH}/bin/nvcc")
  set(CMAKE_CUDA_COMPILER "${CUDA_PATH}/bin/nvcc" CACHE FILEPATH "CUDA nvcc compiler")
  set(CUDAToolkit_ROOT "${CUDA_PATH}" CACHE PATH "CUDAToolkit root (for find_package)")
else()
  # CUDA_PATH 仅含目标库：用 host 的 nvcc + 包装脚本，并让 FindCUDAToolkit 在该路径找 cudart 等库
  if(CROSS_TARGET_ARCH STREQUAL "aarch64" AND EXISTS "${CUDA_PATH}/include")
    if(NOT HOST_NVCC)
      set(HOST_NVCC "/usr/local/cuda/bin/nvcc" CACHE FILEPATH "Host nvcc when CUDA_PATH has no nvcc")
    endif()
    if(NOT EXISTS "${HOST_NVCC}")
      message(FATAL_ERROR "CUDA_PATH has no nvcc and HOST_NVCC=${HOST_NVCC} not found. Set -DHOST_NVCC=/path/to/nvcc")
    endif()
    set(_nvcc_wrapper "${CMAKE_BINARY_DIR}/nvcc_cuda_wrapper.sh")
    file(WRITE "${_nvcc_wrapper}"
      "#!/bin/sh\n"
      "exec \"${HOST_NVCC}\" -I\"${CUDA_PATH}/include\" -L\"${CUDA_PATH}/lib\" \"\$@\"\n")
    file(CHMOD "${_nvcc_wrapper}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
    unset(CMAKE_CUDA_COMPILER CACHE)
    set(CMAKE_CUDA_COMPILER "${_nvcc_wrapper}" CACHE FILEPATH "CUDA nvcc (wrapper for target include)" FORCE)
    # 使 find_package(CUDAToolkit) 在 CUDA_PATH 下查找 cudart 等库（该路径无 nvcc 但含 lib/include）
    set(CUDAToolkit_ROOT "${CUDA_PATH}" CACHE PATH "CUDAToolkit root (target libs when CUDA_PATH has no nvcc)")
    message(STATUS "Using nvcc wrapper for target include: ${_nvcc_wrapper}")
  endif()
endif()

project(cuda_samples LANGUAGES CXX CUDA C)

set(CMAKE_CXX_STANDARD 11)

# -----------------------------------------------------------------------------
# 交叉编译目标架构（可选：aarch64 / x86_64，空则使用本机架构）
# -----------------------------------------------------------------------------
set(CROSS_TARGET_ARCH "" CACHE STRING "Cross-compile target: aarch64, x86_64, or empty for native")
if(CROSS_TARGET_ARCH STREQUAL "aarch64")
  set(CMAKE_SYSTEM_PROCESSOR aarch64)
  set(CUDA_TARGET_ARCH "aarch64")
  # 使用 toolchain 时由 toolchain 设置编译器；若未使用 toolchain 则在此指定 host 编译器供 nvcc 使用
  if(NOT CMAKE_CROSSCOMPILING)
    set(CMAKE_CUDA_HOST_COMPILER "${CMAKE_CXX_COMPILER}" CACHE STRING "Host compiler for nvcc")
    if(NOT CMAKE_CUDA_HOST_COMPILER)
      set(CMAKE_CUDA_HOST_COMPILER "aarch64-linux-gnu-g++" CACHE STRING "Host compiler for nvcc")
    endif()
  endif()
elseif(CROSS_TARGET_ARCH STREQUAL "x86_64")
  set(CMAKE_SYSTEM_PROCESSOR x86_64)
  set(CUDA_TARGET_ARCH "x86_64")
else()
  # 本机
  set(CUDA_TARGET_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
endif()

# -----------------------------------------------------------------------------
# CUDA 架构（传给 nvcc 的 target GPU，对应 -gencode arch=compute_XX,code=sm_XX）
# 子目录里通过 set_target_properties(.. CUDA_ARCHITECTURES "${CUDA_ARCH_LIST}") 传给 nvcc
# Jetson Orin NX = SM 8.7 → 仅编译 Orin 时可设 -DCUDA_ARCH_LIST=87 加快构建
# -----------------------------------------------------------------------------
if(DEFINED CUDA_ARCH_LIST AND NOT "${CUDA_ARCH_LIST}" STREQUAL "")
  # 用户已通过 -DCUDA_ARCH_LIST=87 等指定，保持不改
  set(CUDA_ARCH_LIST "${CUDA_ARCH_LIST}" CACHE STRING "CUDA SM architectures (semicolon-separated)")
elseif(CUDA_TARGET_ARCH MATCHES "aarch64|armv7l|sbsa")
  # aarch64/Jetson 默认多架构（含 87=Orin）；仅 Orin NX 可改为 87
  set(CUDA_ARCH_LIST "53;61;70;72;75;80;86;87;90" CACHE STRING "CUDA SM architectures (semicolon-separated)")
else()
  set(CUDA_ARCH_LIST "50;52;60;61;70;75;80;86;89;90" CACHE STRING "CUDA SM architectures (semicolon-separated)")
endif()

# 可选：目标根文件系统（交叉编译时若需 sysroot）
set(TARGET_FS "" CACHE PATH "Target sysroot for cross-compile (optional)")

# 当 CUDA_PATH 仅含目标库（无 nvcc）时，供子目录用目标头与库
if(CROSS_TARGET_ARCH STREQUAL "aarch64" AND CUDA_PATH AND EXISTS "${CUDA_PATH}/lib" AND NOT EXISTS "${CUDA_PATH}/bin/nvcc")
  set(CUDA_TARGET_LIB_DIR "${CUDA_PATH}/lib")
  if(EXISTS "${CUDA_PATH}/include")
    set(CUDA_TARGET_INCLUDE_DIR "${CUDA_PATH}/include")
  endif()
endif()

# 公共 include 目录（本仓库下的 include，对应原 Makefile 中的 -I../../../Common）
set(CUDA_SAMPLES_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
if(NOT EXISTS "${CUDA_SAMPLES_INCLUDE_DIR}/helper_cuda.h")
  message(FATAL_ERROR "CUDA helper include not found: ${CUDA_SAMPLES_INCLUDE_DIR}/helper_cuda.h")
endif()


add_subdirectory(bandwidthTest)
add_subdirectory(deviceQuery)
add_subdirectory(deviceQueryDrv)
add_subdirectory(convolutionSeparable)
