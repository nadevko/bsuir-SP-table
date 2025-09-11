cmake_minimum_required(VERSION 3.31.7)
project(bsuir-sp C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(SDL3 REQUIRED)

add_executable(bsuir-sp src/main.c)
target_link_libraries(bsuir-sp PRIVATE SDL3::SDL3)
