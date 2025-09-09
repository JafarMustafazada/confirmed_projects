cmake_minimum_required(VERSION 3.19)
project(oglproj1)
set(CMAKE_CXX_STANDARD 20)
# To Build In Linux: sudo apt-get install build-essential cmake libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev


### Add the subdirectories ### __________________________________________________________
# set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)
set(GLFW_BUILD_EXAMPLES   OFF CACHE BOOL "Build the GLFW example programs" FORCE)
set(GLFW_BUILD_TESTS      OFF CACHE BOOL "Build the GLFW test programs" FORCE)
set(GLFW_BUILD_DOCS       OFF CACHE BOOL "Build the GLFW documentation" FORCE)
# Following only for linux
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
add_subdirectory(src/glfw1)
### _____________________________________________________________________________________
add_subdirectory(src/glad1)
### _____________________________________________________________________________________


### Create the executable ### ___________________________________________________________
file(GLOB_RECURSE MY_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_executable(oglproj1  ${MY_SOURCES})
set(MAIN_BINARY_OUTPUT_DIR $<TARGET_FILE_DIR:oglproj1>)
### _____________________________________________________________________________________


### Links to the executable ### _________________________________________________________
find_package(OpenGL REQUIRED)
target_link_libraries(oglproj1 PRIVATE glfw OpenGL::GL)
### _____________________________________________________________________________________
target_link_libraries(oglproj1 PRIVATE glad1)
### _____________________________________________________________________________________


### Set includde directories ### ________________________________________________________
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")
### _____________________________________________________________________________________
target_include_directories(oglproj1 PRIVATE ${CMAKE_SOURCE_DIR}/src/glfw1/include)
### _____________________________________________________________________________________
target_include_directories(oglproj1 PRIVATE ${CMAKE_SOURCE_DIR}/src/glad1/include)
### _____________________________________________________________________________________