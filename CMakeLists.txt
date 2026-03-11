cmake_minimum_required(VERSION 3.10)
project(Aria2Benchmark)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(ARIA2 REQUIRED libaria2)

include_directories(${ARIA2_INCLUDE_DIRS})
link_directories(${ARIA2_LIBRARY_DIRS})

add_executable(benchmark main.cpp)
target_link_libraries(benchmark ${ARIA2_LIBRARIES})
