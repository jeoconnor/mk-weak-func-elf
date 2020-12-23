cmake_minimum_required(VERSION 3.10) 
project("C test cases")

set(CMAKE_VERBOSE_MAKEFILE ON)

set(LINKER_LANGUAGE CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PRELINK_COMMAND cmake-link-wrapper.py)
set(CMAKE_C_LINK_EXECUTABLE "${PRELINK_COMMAND} ${CMAKE_C_LINK_EXECUTABLE}")
set(CMAKE_CXX_LINK_EXECUTABLE "${PRELINK_COMMAND} ${CMAKE_CXX_LINK_EXECUTABLE}")

add_executable(test-link test-single-func.c func.c)

add_executable(test-section-func test-multi-func.c func-section.c func.c)
target_compile_options(test-section-func PRIVATE -DCUSTOM_SECTION=.mock)
