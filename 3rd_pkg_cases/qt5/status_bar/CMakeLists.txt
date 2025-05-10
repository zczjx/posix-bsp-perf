cmake_minimum_required(VERSION 3.5)

project(status_bar VERSION 0.1 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(QT_VERSION_MAJOR 5)

set(ENV{PKG_CONFIG_PATH} "/opt/cross_env/rk3588s/install/lib/pkgconfig")
message(STATUS "ENV{PKG_CONFIG_PATH}: $ENV{PKG_CONFIG_PATH}")

find_package(PkgConfig REQUIRED)
pkg_check_modules(QT5_WIDGETS REQUIRED Qt5Widgets)

# 1. 设置主机 moc/uic 路径
set(MOC_EXEC /usr/bin/moc)
set(UIC_EXEC /usr/bin/uic)

add_custom_command(
    OUTPUT moc_main_window.cpp
    COMMAND ${MOC_EXEC} ${CMAKE_CURRENT_SOURCE_DIR}/main_window.h -o moc_main_window.cpp
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/main_window.h
    COMMENT "Running moc on main_window.h"
)

add_custom_command(
    OUTPUT moc_status_bar.cpp
    COMMAND ${MOC_EXEC} ${CMAKE_CURRENT_SOURCE_DIR}/status_bar.h -o moc_status_bar.cpp
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/status_bar.h
    COMMENT "Running moc on status_bar.h"
)

add_custom_command(
    OUTPUT ui_main_window.h
    COMMAND ${UIC_EXEC} ${CMAKE_CURRENT_SOURCE_DIR}/main_window.ui -o ui_main_window.h
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/main_window.ui
    COMMENT "Running uic on main_window.ui"
)

add_custom_target(qt_generated_files DEPENDS
        moc_main_window.cpp
        moc_status_bar.cpp
        ui_main_window.h
)



set(PROJECT_SOURCES
        main.cpp
        main_window.cpp
        main_window.h
        status_bar.cpp
        status_bar.h
        ui_main_window.h
        moc_main_window.cpp
        moc_status_bar.cpp
)


add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})
add_dependencies(${PROJECT_NAME} qt_generated_files)

set_target_properties(${PROJECT_NAME} PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    INSTALL_RPATH "$ORIGIN/../lib"
    LINK_FLAGS "-Wl,-rpath-link,/opt/cross_env/rk3588s/install/lib"
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    ${QT5_WIDGETS_LDFLAGS}
)
target_include_directories(${PROJECT_NAME} PUBLIC ${QT5_WIDGETS_INCLUDE_DIRS})
