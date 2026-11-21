cmake_minimum_required(VERSION 3.12)
project(feature_fusion VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

# Eigen 由编译环境(Docker)提供。
# 交叉编译时 Eigen 与其他三方库同处一个 install 前缀:
#   ${BSP_LIB_PATH}      = <prefix>/lib
#   Eigen3Config.cmake   = <prefix>/share/eigen3/cmake
# 未指定 BSP_LIB_PATH 时使用系统默认(libeigen3-dev)。
if(DEFINED BSP_LIB_PATH AND NOT Eigen3_DIR)
    get_filename_component(_bsp_prefix "${BSP_LIB_PATH}" DIRECTORY)
    set(Eigen3_DIR "${_bsp_prefix}/share/eigen3/cmake")
endif()

find_package(Eigen3 REQUIRED NO_MODULE)
message(STATUS "feature_fusion: Eigen3 ${Eigen3_VERSION} (${Eigen3_DIR})")

add_library(${PROJECT_NAME} SHARED
    localization/GnssImuLocalizer.cpp
)

# 头文件按 "fusion/..." 与库内相对路径 "core/..." 两种方式都能引用
target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE Eigen3::Eigen)

set_target_properties(${PROJECT_NAME} PROPERTIES
    INSTALL_RPATH "$ORIGIN/../lib"
)

install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION lib
)
