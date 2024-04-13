cmake_minimum_required(VERSION 3.12)
project(my_project)

set(CMAKE_CXX_STANDARD 11)

# Set the cross toolchain prefix
set(CMAKE_CXX_COMPILER_PREFIX aarch64-linux-gnu-)
# Set the source files
set(SOURCES
    src/main.cpp
    src/foo.cpp
    src/bar.cpp
)

# Add subdirectories for each subproject
add_subdirectory(common-case)
add_subdirectory(exynos-4412)
add_subdirectory(rpi)

# Create the executable
add_executable(my_project ${SOURCES})

# Set the include directories
target_include_directories(my_project PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    /usr/include/glib-2.0
    /usr/lib/aarch64-linux-gnu/glib-2.0/include
)

# Set the link libraries
target_link_libraries(my_project PRIVATE
    common_bsp
    glib-2.0
    gmodule-2.0
    gobject-2.0
    gio-2.0
    gthread-2.0
    m
    pthread
)

# Set the output directory
set_target_properties(my_project PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin
)

# Clean target
add_custom_target(clean-all
    COMMAND ${CMAKE_BUILD_TOOL} clean
    COMMAND rm -rf ${CMAKE_CURRENT_SOURCE_DIR}/bin
)