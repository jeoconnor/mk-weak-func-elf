cmake_minimum_required(VERSION 3.10.0) 
project("test-double-elfpatch")

set(BINARY ${CMAKE_PROJECT_NAME})

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_INSTALL_PREFIX "")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
# Need to set to 20 to run gdb
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_C_FLAGS "-ggdb -Wall")
set(CMAKE_CXX_FLAGS "-ggdb -Wall")

add_executable(${BINARY} main.cpp)

target_link_options(${BINARY} PRIVATE -ggdb -lelf)

install(TARGETS ${BINARY})

# these are compile tests: looking for failure to link
add_subdirectory(test)
