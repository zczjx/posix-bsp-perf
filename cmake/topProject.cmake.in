# 设置CMake最小版本
cmake_minimum_required(VERSION 3.12)

# 设置项目名称
project(PosixPerf)

# Set the language version
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/install")

# 为不同的构建类型设置编译选项
if(CMAKE_BUILD_TYPE MATCHES "Debug")
    # Debug 构建选项
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -rdynamic -g -Wall -Wextra")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -rdynamic -g -Wall -Wextra")
elseif(CMAKE_BUILD_TYPE MATCHES "Release")
    # Release 构建选项
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -Wall -Wextra")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -Wall -Wextra")
endif()

# 添加子目录
add_subdirectory(src)
add_subdirectory(perf_cases)


# Clean target
add_custom_target(clean-all
    COMMAND ${CMAKE_BUILD_TOOL} clean
    COMMAND rm -rf ${CMAKE_INSTALL_PREFIX}
)
