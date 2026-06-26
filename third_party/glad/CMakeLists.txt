cmake_minimum_required(VERSION 3.14)
set(CMAKE_C_STANDARD 17)
project(glad)
add_library(glad STATIC src/gl.c)
target_include_directories(glad PUBLIC 
 ${CMAKE_CURRENT_SOURCE_DIR}/include
)