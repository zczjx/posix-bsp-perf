# Set the minimum required version of CMake
cmake_minimum_required(VERSION 3.12)

# Set the project name
project(bsp_g2d VERSION 0.0.1 LANGUAGES CXX)

# Set the language version
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Add the source files
set(SOURCES
  impl/IGraphics2D.cpp
)


if(BUILD_PLATFORM_RK35XX)
  add_compile_definitions(BUILD_PLATFORM_RK35XX)
  list(APPEND SOURCES
    impl/rk_rga/rkrga.cpp
  )
endif()

if(BUILD_PLATFORM_JETSON)
  add_compile_definitions(BUILD_PLATFORM_JETSON)
  list(APPEND SOURCES
    impl/nv_vic/NvVicGraphics2D.cpp
  )
endif()

# Add the library target
add_library(${PROJECT_NAME} SHARED ${SOURCES})

if(BUILD_PLATFORM_RK35XX)
  if(DEFINED BSP_LIB_PATH)
    set(RKRGA_LIB ${BSP_LIB_PATH}/librga.so)
  else()
    set(RKRGA_LIB /usr/lib/librga.so)
  endif()
  target_link_libraries(${PROJECT_NAME} PUBLIC ${RKRGA_LIB})
endif()

if(BUILD_PLATFORM_JETSON)
  # Find NVIDIA libraries for Jetson
    # Default paths for Jetson platforms - try nvidia subdirectory first
  find_library(NVBUFSURFACE_LIB nvbufsurface
      PATHS
      /usr/lib/aarch64-linux-gnu/nvidia
      /usr/lib/aarch64-linux-gnu
      /usr/lib
      NO_DEFAULT_PATH
  )
  find_library(NVBUFSURFTRANSFORM_LIB nvbufsurftransform
     PATHS
      /usr/lib/aarch64-linux-gnu/nvidia
      /usr/lib/aarch64-linux-gnu
      /usr/lib
    NO_DEFAULT_PATH
  )


  if(NVBUFSURFACE_LIB)
    message(STATUS "Found nvbufsurface: ${NVBUFSURFACE_LIB}")
    target_link_libraries(${PROJECT_NAME} PUBLIC ${NVBUFSURFACE_LIB})
  else()
    message(WARNING "nvbufsurface library not found, linking by name")
    target_link_libraries(${PROJECT_NAME} PUBLIC nvbufsurface)
  endif()

  if(NVBUFSURFTRANSFORM_LIB)
    message(STATUS "Found nvbufsurftransform: ${NVBUFSURFTRANSFORM_LIB}")
    target_link_libraries(${PROJECT_NAME} PUBLIC ${NVBUFSURFTRANSFORM_LIB})
  else()
    message(WARNING "nvbufsurftransform library not found, linking by name")
    target_link_libraries(${PROJECT_NAME} PUBLIC nvbufsurftransform)
  endif()

  # Add Jetson multimedia API include paths
  target_include_directories(${PROJECT_NAME} PRIVATE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/jetson_multimedia_api/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
  )
endif()

# target_link_libraries(${PROJECT_NAME} PRIVATE bsp_shared)

target_include_directories(${PROJECT_NAME} PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include>
)

# 指定pkgconfig文件的内容
set(${PROJECT_NAME}_PC "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc")

# 配置pkgconfig文件
configure_file(${CMAKE_SOURCE_DIR}/cmake/subProject.pc.in ${${PROJECT_NAME}_PC} @ONLY)

# 安装pkgconfig文件
install(FILES ${${PROJECT_NAME}_PC} DESTINATION lib/pkgconfig)


set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")
target_link_libraries(${PROJECT_NAME} PRIVATE bsp_shared)

install(TARGETS ${PROJECT_NAME}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
)


install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
  DESTINATION include/${CMAKE_PROJECT_NAME}
  FILES_MATCHING
  PATTERN "*.h"
  PATTERN "*.hpp"
)

