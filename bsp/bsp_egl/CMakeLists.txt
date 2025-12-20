cmake_minimum_required(VERSION 3.12)

project(bsp_egl VERSION 0.0.1 LANGUAGES CXX)

# 查找依赖库
find_package(PkgConfig REQUIRED)
pkg_check_modules(X11 REQUIRED x11)
# EGL和OpenGL ES库
pkg_check_modules(EGL REQUIRED egl)
pkg_check_modules(GLES2 REQUIRED glesv2)

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
        ${X11_INCLUDE_DIRS}
)

# 链接库（EGL、OpenGL ES 2.0和X11）
target_link_libraries(${PROJECT_NAME}
    PUBLIC
    ${EGL_LDFLAGS}
    ${GLES2_LDFLAGS}
    ${X11_LDFLAGS}
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
