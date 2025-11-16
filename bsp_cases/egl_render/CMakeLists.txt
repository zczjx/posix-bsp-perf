cmake_minimum_required(VERSION 3.12)

project(egl_render)

# 源文件
set(EGL_RENDER_SOURCES
    main.cpp
    EglRenderApp.cpp
)

# 创建可执行文件
add_executable(egl_render ${EGL_RENDER_SOURCES})

# 设置包含目录
target_include_directories(egl_render
    PRIVATE
        ${CMAKE_SOURCE_DIR}/bsp
)

# 链接库
target_link_libraries(egl_render
    PRIVATE
        bsp_egl
        case_framework
        bsp_shared
)

set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")

# 安装可执行文件
install(TARGETS egl_render
    RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
)

# 安装配置文件
install(FILES egl_render.ini
    DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
)

message(STATUS "egl_render application configured")

