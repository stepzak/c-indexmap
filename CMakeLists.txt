cmake_minimum_required(VERSION 3.30)
project(indexmap C)

set(CMAKE_C_STANDARD 23)

add_subdirectory(external/cvec)

add_library(indexmap STATIC
        src/im.c
        include/im.h
)

target_link_libraries(indexmap PUBLIC cvec)

target_include_directories(indexmap PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/external/cvec/include"
)

add_subdirectory(examples)