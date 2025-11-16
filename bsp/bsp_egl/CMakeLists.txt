cmake_minimum_required(VERSION 3.12)

project(bsp_egl VERSION 0.0.1 LANGUAGES CXX)

# 查找依赖库
find_package(X11 REQUIRED)

# EGL和OpenGL ES库
find_library(EGL_LIBRARY NAMES EGL REQUIRED)
find_library(GLES2_LIBRARY NAMES GLESv2 REQUIRED)

if(NOT EGL_LIBRARY)
    message(FATAL_ERROR "EGL library not found")
endif()

if(NOT GLES2_LIBRARY)
    message(FATAL_ERROR "GLESv2 library not found")
endif()

message(STATUS "Found EGL: ${EGL_LIBRARY}")
message(STATUS "Found GLESv2: ${GLES2_LIBRARY}")
message(STATUS "Found X11: ${X11_LIBRARIES}")

# 源文件
set(SOURCES
    RenderEGL.cpp
    Cpu2dGraphics.cpp
)

# 创建静态库
add_library(${PROJECT_NAME} SHARED ${SOURCES})

# 设置包含目录
target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    PRIVATE
        ${X11_INCLUDE_DIR}
)

# 链接库（EGL、OpenGL ES 2.0和X11）
target_link_libraries(${PROJECT_NAME}
    PUBLIC
        ${EGL_LIBRARY}
        ${GLES2_LIBRARY}
        ${X11_LIBRARIES}
)

# 安装头文件
install(FILES
    RenderEGL.hpp
    Cpu2dGraphics.hpp
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include/${PROJECT_NAME}
)

# 安装库文件
install(TARGETS ${PROJECT_NAME}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
)

message(STATUS "${PROJECT_NAME} library configured")
message(STATUS "  - RenderEGL: EGL + OpenGL ES 2.0 (GPU accelerated)")
message(STATUS "  - Cpu2dGraphics: CPU-based 2D graphics")
