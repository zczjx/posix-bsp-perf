cmake_minimum_required(VERSION 3.12)

project(data_recorder_ui VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(QT_VERSION_MAJOR 5)

if(DEFINED BSP_PKG_CONFIG_PATH)
    # 设置环境变量 PKG_CONFIG_PATH
    set(ENV{PKG_CONFIG_PATH} ${BSP_PKG_CONFIG_PATH})
    message(STATUS "ENV{PKG_CONFIG_PATH}: $ENV{PKG_CONFIG_PATH}")
endif()

# set(ENV{PKG_CONFIG_PATH} "/opt/cross_env/rk3588s/install/lib/pkgconfig")
# message(STATUS "ENV{PKG_CONFIG_PATH}: $ENV{PKG_CONFIG_PATH}")
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(QT5_WIDGETS REQUIRED Qt5Widgets)
pkg_check_modules(PC_MSGPACK REQUIRED msgpack)

# 1. 设置主机 moc/uic 路径
set(MOC_EXEC /usr/bin/moc)
set(UIC_EXEC /usr/bin/uic)

add_custom_command(
    OUTPUT ui_CameraView.h
    COMMAND ${UIC_EXEC} ${CMAKE_CURRENT_SOURCE_DIR}/CameraView.ui -o ui_CameraView.h
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/CameraView.ui
    COMMENT "Running uic on CameraView.ui"
)

add_custom_command(
    OUTPUT moc_VideoFrameWidget.cpp
    COMMAND ${MOC_EXEC} ${CMAKE_CURRENT_SOURCE_DIR}/VideoFrameWidget.h -o moc_VideoFrameWidget.cpp
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/VideoFrameWidget.h
    COMMENT "Running moc on VideoFrameWidget.h"
)

add_custom_command(
    OUTPUT moc_FrameWidget.cpp
    COMMAND ${MOC_EXEC} ${CMAKE_CURRENT_SOURCE_DIR}/FrameWidget.h -o moc_FrameWidget.cpp
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/FrameWidget.h
    COMMENT "Running moc on FrameWidget.h"
)

add_custom_target(qt_generated_files DEPENDS
    ui_CameraView.h
    moc_VideoFrameWidget.cpp
    moc_FrameWidget.cpp
)

set(PROJECT_SOURCES
        VideoFrameWidget.h
        VideoFrameWidget.cpp
        FrameWidget.h
        FrameWidget.cpp
        GuiClient.hpp
        GuiClient.cpp
        main.cpp
        ui_CameraView.h
        moc_VideoFrameWidget.cpp
        moc_FrameWidget.cpp
)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})
add_dependencies(${PROJECT_NAME} qt_generated_files)

set_target_properties(${PROJECT_NAME} PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    INSTALL_RPATH "$ORIGIN/../lib"
    LINK_FLAGS "-Wl,-rpath-link,${QT5_WIDGETS_LIBRARY_DIRS}"
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    ${QT5_WIDGETS_LDFLAGS}
    zeromq_ipc
    bsp_shared
    ${PC_MSGPACK_LDFLAGS}
)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${QT5_WIDGETS_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/midware
    ${CMAKE_SOURCE_DIR}/bsp
    ${CMAKE_SOURCE_DIR}/3rdparty
    ${CMAKE_SOURCE_DIR}/apps/data_recorder
    ${PC_MSGPACK_INCLUDE_DIRS}
)

install(TARGETS ${PROJECT_NAME}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
)