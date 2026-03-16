cmake_minimum_required(VERSION 3.10)

project(oz VERSION 0.0.1 LANGUAGES C CXX DESCRIPTION "oz")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(WIN32)
    add_definitions(-DNOMINMAX)
endif()

add_subdirectory(oz)