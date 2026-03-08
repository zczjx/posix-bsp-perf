# convolutionSeparable - 可分离卷积（参考本目录 Makefile，不修改原 Makefile）
cmake_minimum_required(VERSION 3.18)

if(NOT CUDA_TARGET_LIB_DIR)
  find_package(CUDAToolkit REQUIRED)
endif()

add_executable(convolutionSeparable
  convolutionSeparable.cu
  convolutionSeparable_gold.cpp
  main.cpp
)

target_include_directories(convolutionSeparable PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CUDA_SAMPLES_INCLUDE_DIR}
)
if(CUDA_TARGET_INCLUDE_DIR)
  target_include_directories(convolutionSeparable PRIVATE ${CUDA_TARGET_INCLUDE_DIR})
endif()

if(CUDA_TARGET_LIB_DIR)
  target_link_directories(convolutionSeparable PRIVATE ${CUDA_TARGET_LIB_DIR})
  target_link_libraries(convolutionSeparable PRIVATE cudart)
  if(CUDA_TOOLCHAIN_RPATH_LINK)
    target_link_options(convolutionSeparable PRIVATE ${CUDA_TOOLCHAIN_RPATH_LINK})
  endif()
else()
  target_link_libraries(convolutionSeparable PRIVATE CUDA::cudart)
endif()

set_target_properties(convolutionSeparable PROPERTIES
  CUDA_ARCHITECTURES "${CUDA_ARCH_LIST}"
  CUDA_STANDARD 11
)

if(CMAKE_CUDA_HOST_COMPILER)
  set_target_properties(convolutionSeparable PROPERTIES
    CUDA_HOST_COMPILER "${CMAKE_CUDA_HOST_COMPILER}"
  )
endif()
