cmake_minimum_required(VERSION 3.19)
project(glad1)


set(CMAKE_CXX_STANDARD 20)


### Set includde directories ###
include_directories(glad1 PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include)


### Create the executable ###
file(GLOB_RECURSE MY_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
add_library(glad1 STATIC ${MY_SOURCES})