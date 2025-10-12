cmake_minimum_required(VERSION 3.10)
project(bsuir-sp C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(SDL3 REQUIRED)
find_package(SDL3_ttf REQUIRED)
find_package(Fontconfig REQUIRED)

if(NOT SDL3_FOUND)
  message(FATAL_ERROR "SDL3 not found. Please ensure SDL3 is installed.")
endif()

if(NOT SDL3_ttf_FOUND)
  message(FATAL_ERROR "SDL3_ttf not found. Please ensure SDL3_ttf is installed.")
endif()

if(NOT Fontconfig_FOUND)
  message(FATAL_ERROR "Fontconfig not found. Please ensure Fontconfig is installed.")
endif()

add_executable(bsuir-sp
  src/main.c
  src/globals.c
  src/utils.c
  src/grid.c
  src/fs.c
)

target_include_directories(bsuir-sp PRIVATE
  ${SDL3_INCLUDE_DIRS}
  ${SDL3_ttf_INCLUDE_DIRS}
  ${Fontconfig_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(bsuir-sp PRIVATE
  SDL3::SDL3
  SDL3_ttf::SDL3_ttf
  Fontconfig::Fontconfig
)

# default install dir to build/install if user didn't set it
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "Install path prefix" FORCE)
endif()

install(TARGETS bsuir-sp RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
