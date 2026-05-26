cmake_minimum_required(VERSION 3.12)
project(svs_view VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

add_executable(${PROJECT_NAME}
    SvsViewApp.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/features
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    feature_svs
)

set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)
