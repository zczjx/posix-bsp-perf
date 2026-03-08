# aarch64 交叉编译 toolchain（与 -DCMAKE_TOOLCHAIN_FILE=... 配合使用）
# 用法示例:
#   cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-linux-gnu.cmake
#   cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-linux-gnu.cmake -DCUDA_PATH=/path/to/target/cuda -DTARGET_FS=/path/to/rootfs
#
# 若 CUDA_PATH 仅含 aarch64 的 include/lib（无 nvcc），本 toolchain 会生成 nvcc 包装脚本并设为
# CMAKE_CUDA_COMPILER，使编译器检测和后续编译都使用该路径的头文件。

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# 供顶层 CMakeLists.txt 识别为 aarch64 交叉
set(CROSS_TARGET_ARCH aarch64 CACHE STRING "Cross-compile target (set by toolchain)")

# 可选：仅针对 Jetson Orin NX（SM 8.7）可在此或配置时设 -DCUDA_ARCH_LIST=87，减少编译架构
# 不设置则使用顶层 CMakeLists.txt 的 aarch64 默认多架构列表

# 交叉编译器（可按需改为完整路径）
set(CMAKE_C_COMPILER   aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

# nvcc 使用的 host 编译器必须与目标一致
set(CMAKE_CUDA_HOST_COMPILER "${CMAKE_CXX_COMPILER}" CACHE STRING "Host compiler for nvcc")

# CUDA_PATH 仅含目标库（无 nvcc）时：用包装脚本强制 nvcc 使用该路径的 include（-DCUDA_PATH=... 会入 cache，此处用 ${CUDA_PATH}）
if(CUDA_PATH AND NOT "${CUDA_PATH}" STREQUAL "")
  if(NOT EXISTS "${CUDA_PATH}/bin/nvcc" AND EXISTS "${CUDA_PATH}/include")
    set(_host_nvcc "/usr/local/cuda/bin/nvcc")
    if(DEFINED HOST_NVCC AND EXISTS "${HOST_NVCC}")
      set(_host_nvcc "${HOST_NVCC}")
    endif()
    set(_wrapper "${CMAKE_BINARY_DIR}/nvcc_cuda_wrapper.sh")
    file(WRITE "${_wrapper}"
      "#!/bin/sh\n"
      "exec \"${_host_nvcc}\" -I\"${CUDA_PATH}/include\" -L\"${CUDA_PATH}/lib\" \"\$@\"\n")
    file(CHMOD "${_wrapper}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
    set(CMAKE_CUDA_COMPILER "${_wrapper}" CACHE FILEPATH "CUDA nvcc (wrapper for target include)" FORCE)
  endif()
endif()

# 可选：目标根文件系统（若设 TARGET_FS 则同时作为 sysroot 并追加到 rpath-link，用于 find_package 等）
if(DEFINED TARGET_FS AND TARGET_FS)
  set(CMAKE_SYSROOT "${TARGET_FS}" CACHE PATH "Target sysroot")
  set(CMAKE_FIND_ROOT_PATH "${TARGET_FS}" CACHE PATH "Find root path")
  set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
  set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
  set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
  set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
endif()
