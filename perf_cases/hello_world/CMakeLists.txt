cmake_minimum_required(VERSION 3.10)
project(HelloWorld VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)

add_executable(${PROJECT_NAME} main.cpp HelloPerf.cpp)

# Set the compile options
target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
    -rdynamic
    -g
    # Add more compile options here if needed
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)

set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/../lib/${CMAKE_PROJECT_NAME}")

target_link_libraries(${PROJECT_NAME} PRIVATE case_framework)



install(TARGETS ${PROJECT_NAME}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
)
