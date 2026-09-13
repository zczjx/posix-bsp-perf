cmake_minimum_required(VERSION 3.12)

project(data_recorder_ui VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(DEFINED BSP_PKG_CONFIG_PATH)
    set(ENV{PKG_CONFIG_PATH} ${BSP_PKG_CONFIG_PATH})
    message(STATUS "ENV{PKG_CONFIG_PATH}: $ENV{PKG_CONFIG_PATH}")
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(QT5_CORE REQUIRED Qt5Core)
pkg_check_modules(QT5_WIDGETS REQUIRED Qt5Widgets)
pkg_check_modules(PC_MSGPACK REQUIRED msgpack)

if(DEFINED BSP_LIB_PATH)
    set(OpenCV_DIR ${BSP_LIB_PATH}/cmake/opencv4)
else()
    set(OpenCV_DIR /usr/lib/cmake/opencv4)
endif()
find_package(OpenCV REQUIRED)

# ---------------------------------------------------------------------------
# Qt code generation
#
# AUTOMOC/AUTOUIC are not usable here: they need the Qt5::moc imported target,
# which only find_package(Qt5) provides, and this project resolves Qt through
# pkg-config so it can link a target sysroot. moc and uic must therefore be the
# host binaries, overridable for environments that do not keep them in /usr/bin.
# ---------------------------------------------------------------------------
set(BSP_QT_MOC_EXECUTABLE "/usr/bin/moc" CACHE FILEPATH "Host Qt moc executable")
set(BSP_QT_UIC_EXECUTABLE "/usr/bin/uic" CACHE FILEPATH "Host Qt uic executable")

# Generates moc_<name>.cpp for a Q_OBJECT header and appends it to ${out_var}.
function(bsp_qt_moc header out_var)
    get_filename_component(basename ${header} NAME_WE)
    set(generated ${CMAKE_CURRENT_BINARY_DIR}/moc_${basename}.cpp)
    add_custom_command(
        OUTPUT ${generated}
        COMMAND ${BSP_QT_MOC_EXECUTABLE}
                -I${CMAKE_CURRENT_SOURCE_DIR}
                ${CMAKE_CURRENT_SOURCE_DIR}/${header}
                -o ${generated}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${header}
        COMMENT "moc ${header}"
        VERBATIM
    )
    set(${out_var} ${${out_var}} ${generated} PARENT_SCOPE)
endfunction()

# Generates ui_<name>.h for a .ui form and appends it to ${out_var}.
function(bsp_qt_uic form out_var)
    get_filename_component(basename ${form} NAME_WE)
    set(generated ${CMAKE_CURRENT_BINARY_DIR}/ui_${basename}.h)
    add_custom_command(
        OUTPUT ${generated}
        COMMAND ${BSP_QT_UIC_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/${form} -o ${generated}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${form}
        COMMENT "uic ${form}"
        VERBATIM
    )
    set(${out_var} ${${out_var}} ${generated} PARENT_SCOPE)
endfunction()

# Every header declaring Q_OBJECT. Adding a class means adding one line here.
set(QT_MOC_HEADERS
    controller/GuiController.hpp
    model/FrameHub.hpp
    model/ShmFrameSource.hpp
    view/FrameGridView.hpp
    view/FrameWidget.hpp
    view/RecorderWindow.hpp
)

set(QT_GENERATED_SOURCES)
foreach(header ${QT_MOC_HEADERS})
    bsp_qt_moc(${header} QT_GENERATED_SOURCES)
endforeach()
bsp_qt_uic(view/RecorderWindow.ui QT_GENERATED_SOURCES)

set(PROJECT_SOURCES
    main.cpp
    AppOptions.cpp

    core/Frame.cpp
    core/Logging.cpp

    config/UiConfig.cpp

    model/CameraFrameDecoder.cpp
    model/DetectionFrameDecoder.cpp
    model/FrameHub.cpp
    model/FramePayload.cpp
    model/Recorder.cpp
    model/ShmFrameSource.cpp

    view/FrameGridView.cpp
    view/FrameImage.cpp
    view/FrameWidget.cpp
    view/RecorderWindow.cpp

    controller/GuiController.cpp
)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES} ${QT_GENERATED_SOURCES})

set_target_properties(${PROJECT_NAME} PROPERTIES
    INSTALL_RPATH "$ORIGIN/../lib"
    LINK_FLAGS "-Wl,-rpath-link,${QT5_WIDGETS_LIBRARY_DIRS} -Wl,-rpath-link,${OpenCV_LIBRARY_DIRS}"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
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

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}   # "core/Frame.hpp" and friends
    ${CMAKE_CURRENT_BINARY_DIR}   # generated ui_*.h
    ${QT5_CORE_INCLUDE_DIRS}
    ${QT5_WIDGETS_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/midware
    ${CMAKE_SOURCE_DIR}/bsp
    ${CMAKE_SOURCE_DIR}/3rdparty
    ${CMAKE_SOURCE_DIR}/apps/data_recorder
    ${PC_MSGPACK_INCLUDE_DIRS}
    ${OpenCV_INCLUDE_DIRS}
)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
)
