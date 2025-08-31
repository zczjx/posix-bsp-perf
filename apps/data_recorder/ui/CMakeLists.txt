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

set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(QT5_CORE REQUIRED Qt5Core)
pkg_check_modules(QT5_WIDGETS REQUIRED Qt5Widgets)
pkg_check_modules(PC_MSGPACK REQUIRED msgpack)

if(DEFINED BSP_LIB_PATH)
  set(OpenCV_DIR ${BSP_LIB_PATH}/cmake/opencv4)
else()
    set(OpenCV_DIR /usr/lib/cmake/opencv4)
endif()
# Find OpenCV package
find_package(OpenCV REQUIRED)

# 1. 设置主机 moc/uic 路径
set(MOC_EXEC /usr/bin/moc)
set(UIC_EXEC /usr/bin/uic)

# 2. UI文件处理
add_custom_command(
    OUTPUT view/ui_CameraView.h
    COMMAND ${UIC_EXEC} ${CMAKE_CURRENT_SOURCE_DIR}/view/CameraView.ui -o ${CMAKE_CURRENT_SOURCE_DIR}/view/ui_CameraView.h
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/view/CameraView.ui
    COMMENT "Running uic on CameraView.ui"
)

# 3. MOC处理 - View类
add_custom_command(
    OUTPUT moc_VideoFrameWidget.cpp
    COMMAND ${MOC_EXEC} ${CMAKE_CURRENT_SOURCE_DIR}/view/VideoFrameWidget.h -o moc_VideoFrameWidget.cpp
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/view/VideoFrameWidget.h
    COMMENT "Running moc on VideoFrameWidget.h"
)

add_custom_command(
    OUTPUT moc_FrameWidget.cpp
    COMMAND ${MOC_EXEC} ${CMAKE_CURRENT_SOURCE_DIR}/view/FrameWidget.h -o moc_FrameWidget.cpp
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/view/FrameWidget.h
    COMMENT "Running moc on FrameWidget.h"
)

# 4. MOC处理 - Controller类
add_custom_command(
    OUTPUT moc_GuiController.cpp
    COMMAND ${MOC_EXEC} ${CMAKE_CURRENT_SOURCE_DIR}/controller/GuiController.hpp -o moc_GuiController.cpp
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/controller/GuiController.hpp
    COMMENT "Running moc on GuiController.hpp"
)

# 5. MOC处理 - Model类
add_custom_command(
    OUTPUT moc_RawCamera.cpp
    COMMAND ${MOC_EXEC} ${CMAKE_CURRENT_SOURCE_DIR}/model/RawCamera.hpp -o moc_RawCamera.cpp
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/model/RawCamera.hpp
    COMMENT "Running moc on RawCamera.hpp"
)

add_custom_command(
    OUTPUT moc_ObjectsDetection.cpp
    COMMAND ${MOC_EXEC} ${CMAKE_CURRENT_SOURCE_DIR}/model/ObjectsDetection.hpp -o moc_ObjectsDetection.cpp
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/model/ObjectsDetection.hpp
    COMMENT "Running moc on ObjectsDetection.hpp"
)

# 6. 生成文件目标
add_custom_target(qt_generated_files DEPENDS
    view/ui_CameraView.h
    moc_VideoFrameWidget.cpp
    moc_FrameWidget.cpp
    moc_GuiController.cpp
    moc_RawCamera.cpp
    moc_ObjectsDetection.cpp
)

# 7. 项目源文件
set(PROJECT_SOURCES
    # 主程序
    main.cpp

    # Controller层
    controller/GuiController.hpp
    controller/GuiController.cpp
    moc_GuiController.cpp

    # Model层
    model/RawCamera.hpp
    model/RawCamera.cpp
    moc_RawCamera.cpp
    model/ObjectsDetection.hpp
    model/ObjectsDetection.cpp
    moc_ObjectsDetection.cpp
    model/Recorder.hpp
    model/Recorder.cpp

    # View层
    view/VideoFrameWidget.h
    view/VideoFrameWidget.cpp
    moc_VideoFrameWidget.cpp
    view/FrameWidget.h
    view/FrameWidget.cpp
    moc_FrameWidget.cpp
)

# 8. 创建可执行文件
add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})
add_dependencies(${PROJECT_NAME} qt_generated_files)

# 9. 设置目标属性
set_target_properties(${PROJECT_NAME} PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    INSTALL_RPATH "$ORIGIN/../lib"
    LINK_FLAGS "-Wl,-rpath-link,${QT5_WIDGETS_LIBRARY_DIRS} -Wl,-rpath-link,${OpenCV_LIBRARY_DIRS}"
)

# 10. 链接库
target_link_libraries(${PROJECT_NAME} PUBLIC
    ${QT5_CORE_LDFLAGS}
    ${QT5_WIDGETS_LDFLAGS}
    zeromq_ipc
    bsp_shared
    bsp_container
    bsp_enc
    bsp_g2d
    ${PC_MSGPACK_LDFLAGS}
    ${OpenCV_LIBRARIES}
)

# 11. 包含目录
target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}  # 添加构建目录，确保能找到生成的UI文件
    ${CMAKE_CURRENT_SOURCE_DIR}  # 添加当前目录，确保UI文件能找到FrameWidget.h
    ${QT5_CORE_INCLUDE_DIRS}
    ${QT5_WIDGETS_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/midware
    ${CMAKE_SOURCE_DIR}/bsp
    ${CMAKE_SOURCE_DIR}/3rdparty
    ${CMAKE_SOURCE_DIR}/apps/data_recorder
    ${PC_MSGPACK_INCLUDE_DIRS}
    ${OpenCV_INCLUDE_DIRS}
)

# 12. 安装规则
install(TARGETS ${PROJECT_NAME}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
)