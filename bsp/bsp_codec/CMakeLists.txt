cmake_minimum_required(VERSION 3.12)
project(bsp_codec VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(ENC_PRJ_NAME bsp_enc)
set(DEC_PRJ_NAME bsp_dec)

# 定义选项以启用或禁用某些功能
option(ENABLE_RK_MPP "Enable RK_MPP support" OFF)

# 定义源文件列表
set(ENC_SOURCES
    IEncoder.cpp
)

# 根据选项条件性地添加源文件
if(ENABLE_RK_MPP)
    list(APPEND ENC_SOURCES
        video/rkmppEnc.cpp
    )
endif()

# 添加第一个动态库
add_library(${ENC_PRJ_NAME} SHARED ${ENC_SOURCES})

target_include_directories(${ENC_PRJ_NAME} PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include>
)

# 指定pkgconfig文件的内容
set(${ENC_PRJ_NAME}_PC "${CMAKE_CURRENT_BINARY_DIR}/${ENC_PRJ_NAME}.pc")

# 配置pkgconfig文件
configure_file(${CMAKE_SOURCE_DIR}/cmake/subProject.pc.in ${${ENC_PRJ_NAME}_PC} @ONLY)

# 安装pkgconfig文件
install(FILES ${${ENC_PRJ_NAME}_PC} DESTINATION lib/pkgconfig)

set_target_properties(${ENC_PRJ_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")
target_link_libraries(${ENC_PRJ_NAME} PRIVATE bsp_shared)

install(TARGETS ${ENC_PRJ_NAME}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
)


# 如果有第二个动态库，可以继续添加
set(DEC_SOURCES
    IDecoder.cpp
)

if(ENABLE_RK_MPP)
    list(APPEND DEC_SOURCES
        video/rkmppDec.cpp
    )
endif()

# 添加第二个动态库
add_library(${DEC_PRJ_NAME} SHARED ${DEC_SOURCES})

target_include_directories(${DEC_PRJ_NAME} PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include>
)

# 指定pkgconfig文件的内容
set(${DEC_PRJ_NAME}_PC "${CMAKE_CURRENT_BINARY_DIR}/${ENC_PRJ_NAME}.pc")

# 配置pkgconfig文件
configure_file(${CMAKE_SOURCE_DIR}/cmake/subProject.pc.in ${${DEC_PRJ_NAME}_PC} @ONLY)

# 安装pkgconfig文件
install(FILES ${${DEC_PRJ_NAME}_PC} DESTINATION lib/pkgconfig)

set_target_properties(${DEC_PRJ_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")
target_link_libraries(${DEC_PRJ_NAME} PRIVATE bsp_shared)

if(ENABLE_RK_MPP)
  if(DEFINED BSP_LIB_PATH)
    set(RK_MPP_LIB ${BSP_LIB_PATH}/librockchip_mpp.so)
  else()
    set(RK_MPP_LIB /usr/lib/librockchip_mpp.so)
  endif()
  target_link_libraries(${DEC_PRJ_NAME} PUBLIC ${RK_MPP_LIB})
  target_link_libraries(${ENC_PRJ_NAME} PUBLIC ${RK_MPP_LIB})
endif()

install(TARGETS ${DEC_PRJ_NAME}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
  DESTINATION include/${CMAKE_PROJECT_NAME}
  FILES_MATCHING
  PATTERN "*.h"
  PATTERN "*.hpp"
)
