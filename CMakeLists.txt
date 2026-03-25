cmake_minimum_required(VERSION 3.20)
project(tinyvm C)

set(CMAKE_C_STANDARD 11)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.c)
add_executable(tinyvm ${SOURCES})

target_include_directories(tinyvm PRIVATE include)
target_compile_options(tinyvm PRIVATE -Wall -Wextra)
