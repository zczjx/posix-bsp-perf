cmake_minimum_required(VERSION 3.12)

project(bsp_egl)

# 查找依赖库
find_package(X11 REQUIRED)

# EGL库（不需要GLESv2）
find_library(EGL_LIBRARY NAMES EGL)

if(NOT EGL_LIBRARY)
    message(FATAL_ERROR "EGL library not found")
endif()

message(STATUS "Found EGL: ${EGL_LIBRARY}")
message(STATUS "Found X11: ${X11_LIBRARIES}")

# 源文件
set(BSP_EGL_SOURCES
    RenderEGL.cpp
)

# 创建静态库
add_library(bsp_egl STATIC ${BSP_EGL_SOURCES})

# 设置包含目录
target_include_directories(bsp_egl
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    PRIVATE
        ${X11_INCLUDE_DIR}
)

# 链接库（只需要EGL和X11，不需要GLESv2）
target_link_libraries(bsp_egl
    PUBLIC
        ${EGL_LIBRARY}
        ${X11_LIBRARIES}
)

# 安装头文件
install(FILES RenderEGL.hpp
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include/bsp_egl
)

# 安装库文件
install(TARGETS bsp_egl
    ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
)

message(STATUS "bsp_egl library configured (Pure EGL, no OpenGL ES)")
