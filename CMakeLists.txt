cmake_minimum_required(VERSION 3.10) # 3.31.7 слишком новая, 3.10 достаточно
project(bsuir-sp C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(SDL3 REQUIRED)
find_package(SDL3_ttf REQUIRED)
find_package(Fontconfig REQUIRED)

add_executable(bsuir-sp src/main.c src/main.h)

target_link_libraries(bsuir-sp PRIVATE SDL3::SDL3 SDL3_ttf::SDL3_ttf
                                       Fontconfig::Fontconfig)
