# 设置CMake最小版本
cmake_minimum_required(VERSION 3.10)

# 设置项目名称
project(SharedMemoryDetection)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -Wextra")

# 查找依赖包
find_package(OpenCV REQUIRED COMPONENTS core imgproc highgui videoio imgcodecs)
find_package(PkgConfig REQUIRED)

# 查找ZeroMQ
pkg_check_modules(ZMQ REQUIRED libzmq)

# 查找MessagePack
find_path(MSGPACK_INCLUDE_DIR 
    NAMES msgpack.hpp 
    PATHS /usr/include /usr/local/include /opt/homebrew/include
)

if(NOT MSGPACK_INCLUDE_DIR)
    message(FATAL_ERROR "MessagePack not found! Please install msgpack-c-dev or msgpack-cxx")
endif()

# 打印找到的依赖信息
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "OpenCV include dirs: ${OpenCV_INCLUDE_DIRS}")
message(STATUS "ZeroMQ include dirs: ${ZMQ_INCLUDE_DIRS}")
message(STATUS "MessagePack include dir: ${MSGPACK_INCLUDE_DIR}")

# 设置头文件目录
include_directories(
    ${OpenCV_INCLUDE_DIRS}
    ${ZMQ_INCLUDE_DIRS}
    ${MSGPACK_INCLUDE_DIR}
)

# 添加库目录
link_directories(${ZMQ_LIBRARY_DIRS})

# 创建共享内存检测库
add_library(shared_memory_detection SHARED
    shared_memory_detection.cpp
)

target_link_libraries(shared_memory_detection
    ${OpenCV_LIBS}
    ${ZMQ_LIBRARIES}
    rt          # POSIX实时扩展
    pthread     # 线程库
)

# 设置库的编译标志
target_compile_options(shared_memory_detection PRIVATE ${ZMQ_CFLAGS_OTHER})

# 创建发布者示例程序
add_executable(publisher_example
    publisher_example.cpp
)

target_link_libraries(publisher_example
    shared_memory_detection
    ${OpenCV_LIBS}
    ${ZMQ_LIBRARIES}
    rt
    pthread
)

target_compile_options(publisher_example PRIVATE ${ZMQ_CFLAGS_OTHER})

# 创建接收者示例程序
add_executable(receiver_example
    receiver_example.cpp
)

target_link_libraries(receiver_example
    shared_memory_detection
    ${OpenCV_LIBS}
    ${ZMQ_LIBRARIES}
    rt
    pthread
)

target_compile_options(receiver_example PRIVATE ${ZMQ_CFLAGS_OTHER})

# 创建性能测试程序
add_executable(performance_test
    performance_test.cpp
)

target_link_libraries(performance_test
    shared_memory_detection
    ${OpenCV_LIBS}
    ${ZMQ_LIBRARIES}
    rt
    pthread
)

target_compile_options(performance_test PRIVATE ${ZMQ_CFLAGS_OTHER})

# 安装规则
install(TARGETS shared_memory_detection 
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES shared_memory_detection.h 
    DESTINATION include
)

install(TARGETS publisher_example receiver_example performance_test
    RUNTIME DESTINATION bin
)

# 生成pkg-config文件
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/shared_memory_detection.pc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/shared_memory_detection.pc"
    @ONLY
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/shared_memory_detection.pc"
    DESTINATION lib/pkgconfig
)

# 打印编译信息
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ flags: ${CMAKE_CXX_FLAGS}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")

# 添加自定义目标用于清理共享内存
add_custom_target(clean_shm
    COMMAND ${CMAKE_COMMAND} -E echo "清理共享内存对象..."
    COMMAND rm -f /dev/shm/detection_images* || true
    COMMENT "清理遗留的共享内存对象"
)

# 创建卸载目标
if(NOT TARGET uninstall)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
        IMMEDIATE @ONLY
    )
    
    add_custom_target(uninstall
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
    )
endif()

# 添加测试
enable_testing()

# 基本功能测试
add_test(NAME basic_functionality 
    COMMAND ${CMAKE_COMMAND} -E echo "基本功能测试需要手动运行publisher_example和receiver_example"
)

# 编译后提示
add_custom_target(build_complete ALL
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "==============================================="
    COMMAND ${CMAKE_COMMAND} -E echo "编译完成! 可执行文件："
    COMMAND ${CMAKE_COMMAND} -E echo "  ./publisher_example  - 发布者示例"
    COMMAND ${CMAKE_COMMAND} -E echo "  ./receiver_example   - 接收者示例"
    COMMAND ${CMAKE_COMMAND} -E echo "  ./performance_test   - 性能测试"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "使用方法："
    COMMAND ${CMAKE_COMMAND} -E echo "1. 先运行: ./publisher_example"
    COMMAND ${CMAKE_COMMAND} -E echo "2. 再运行: ./receiver_example"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "清理共享内存: make clean_shm"
    COMMAND ${CMAKE_COMMAND} -E echo "==============================================="
    COMMAND ${CMAKE_COMMAND} -E echo ""
    DEPENDS publisher_example receiver_example performance_test
)

# 设置默认构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 调试版本的额外设置
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DDEBUG_MODE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
endif()
