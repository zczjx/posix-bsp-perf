# Set the minimum required version of CMake
cmake_minimum_required(VERSION 3.12)

# Set the project name
project(case_framework VERSION 0.0.1 LANGUAGES CXX)

# Set the language version
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)

# Add the source files
set(SOURCES
    BasePerfCase.hpp
    BasePerfCase.cpp
    # Add more source files here if needed
)

# Add the library target
add_library(${PROJECT_NAME} SHARED ${SOURCES})

# Set the compile options
target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
    # Add more compile options here if needed
)


# Set the compile options
target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
    -rdynamic
    -g
    # Add more compile options here if needed
)

string(COMPARE EQUAL ${PROJECT_NAME} ${CMAKE_PROJECT_NAME} is_top_level)
if(is_top_level)
  message(FATAL_ERROR "This subproject must be built as part of the top-level project.")
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/../lib/${CMAKE_PROJECT_NAME}")

install(TARGETS ${PROJECT_NAME}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib/${CMAKE_PROJECT_NAME}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include>
)

message(STATUS "CMAKE_BINARY_DIR: ${CMAKE_BINARY_DIR}")

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
  DESTINATION include/${CMAKE_PROJECT_NAME}
  FILES_MATCHING
  PATTERN "*.h"
  PATTERN "*.hpp"
)

