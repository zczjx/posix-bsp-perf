cmake_minimum_required(VERSION 3.12)
project(JsonParse VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

add_executable(${PROJECT_NAME} main.cpp)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/bsp ${CMAKE_SOURCE_DIR}/3rdparty)

set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")

target_link_libraries(${PROJECT_NAME} PRIVATE bsp_shared)


install(TARGETS ${PROJECT_NAME}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
)

install(FILES ${CMAKE_SOURCE_DIR}/bsp_cases/json_parse/carla-vehicle-view.json DESTINATION share/bsp_cases/json_parse)