cmake_minimum_required(VERSION 3.15)

project(klang)

## workarounds
#set(FLEX_EXECUTABLE                  /usr/local/opt/flex/bin/flex)                                                                           
#set(FLEX_INCLUDE_DIR                 /usr/local/opt/flex/include)
#set(FL_LIBRARY                       /usr/local/opt/flex/lib/libfl.dylib)

find_package(BISON  REQUIRED)
find_package(FLEX REQUIRED)

set(ASAN_ENABLED OFF) 

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${FLEX_INCLUDE_DIRS})
link_directories(${FLEX_LIBRARIES})

add_subdirectory(compiler)