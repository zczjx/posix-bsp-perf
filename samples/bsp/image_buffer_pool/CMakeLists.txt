cmake_minimum_required(VERSION 3.12)
project(image_buffer_pool VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

add_executable(${PROJECT_NAME} main.cpp)

set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")

target_link_libraries(${PROJECT_NAME} PRIVATE bsp_mem bsp_image)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
)
