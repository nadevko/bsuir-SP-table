cmake_minimum_required(VERSION 3.16)
project(bsuir-sp LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(SDL3 REQUIRED)
find_package(SDL3_ttf REQUIRED)
find_package(Fontconfig REQUIRED)

# Automatically collect all .c files in src directory
file(GLOB SOURCE_FILES "${CMAKE_SOURCE_DIR}/src/*.c")
if(NOT SOURCE_FILES)
  message(FATAL_ERROR "No C source files found in src directory.")
endif()

add_executable(bsuir-sp ${SOURCE_FILES})

target_include_directories(
  bsuir-sp
  PRIVATE ${SDL3_INCLUDE_DIRS} ${SDL3_ttf_INCLUDE_DIRS}
          ${Fontconfig_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/include
          ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(bsuir-sp PRIVATE SDL3::SDL3 SDL3_ttf::SDL3_ttf
                                       Fontconfig::Fontconfig m)

# Default install dir to build/install if user didn't set it
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX
      "${CMAKE_BINARY_DIR}/install"
      CACHE PATH "Install path prefix" FORCE)
endif()

install(TARGETS bsuir-sp RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
