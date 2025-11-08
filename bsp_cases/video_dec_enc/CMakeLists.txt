cmake_minimum_required(VERSION 3.12)
project(video_dec_enc VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE case_framework bsp_shared bsp_enc bsp_dec)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/bsp)
set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")


# 添加 Jetson 平台特定的链接配置
if(BUILD_PLATFORM_JETSON)
  target_link_directories(${PROJECT_NAME} PUBLIC
    /usr/lib/aarch64-linux-gnu/tegra
  )
  target_link_libraries(${PROJECT_NAME} PRIVATE
    nvv4l2
    nvbufsurface
    nvbufsurftransform
  )
endif()

install(TARGETS ${PROJECT_NAME}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
)

