cmake_minimum_required(VERSION 3.13.0) 
project("mock-elfpatch")

set(CMAKE_VERBOSE_MAKEFILE ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(test-cpp-compile test-cpp-compile.cpp)
target_link_options(test-cpp-compile PRIVATE "-static")

add_executable(test-c-compile test-c-compile.c)
target_link_options(test-c-compile PRIVATE "-static")

