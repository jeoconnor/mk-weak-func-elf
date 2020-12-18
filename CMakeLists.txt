cmake_minimum_required(VERSION 3.13.0) 
project("mock-elfpatch")

set(BINARY ${CMAKE_PROJECT_NAME})

set(CMAKE_VERBOSE_MAKEFILE ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_C_FLAGS "-ggdb -Wall")
set(CMAKE_CXX_FLAGS "-ggdb -Wall")

add_executable(${BINARY} ${BINARY}.cpp)

target_link_options(${BINARY} PRIVATE -g -lelf)
