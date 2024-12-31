# Set the minimum required version of CMake
cmake_minimum_required(VERSION 3.12)

# Set the project name
project(perfetto VERSION 0.0.1 LANGUAGES CXX)

# Set the language version
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_INSTALL_PREFIX "./install")

# Add the source files
set(SOURCES
  perfetto.h
  perfetto.cc
    # Add more source files here if needed
)

# Add the library target
add_library(${PROJECT_NAME} STATIC ${SOURCES})
target_compile_options(${PROJECT_NAME} PRIVATE -fPIC)

target_include_directories(${PROJECT_NAME} PRIVATE
    $<INSTALL_INTERFACE:include>
)

install(TARGETS ${PROJECT_NAME}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
