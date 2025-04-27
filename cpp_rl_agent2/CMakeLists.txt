cmake_minimum_required (VERSION 3.10)
project (rl_q_agent2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# include directories
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")
file(GLOB_RECURSE MY_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_executable(rl_q_agent2 ${MY_SOURCES})
