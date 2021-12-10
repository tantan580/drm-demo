cmake_minimum_required(VERSION 2.8)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)
project(drm-singlebuffer)
SET(CMAKE_BUILD_TYPE Debug)

include_directories(/usr/include/libdrm)

add_executable(${PROJECT_NAME} modeset-double-buffer.c)

target_link_libraries(${PROJECT_NAME} PUBLIC
    -ldrm
)

