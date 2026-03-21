cmake_minimum_required(VERSION 3.10)

project(RTK)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(RtkBase src/RtkBase.cpp)
target_link_libraries(RtkBase GnssHat)

add_executable(RtkRover src/RtkRover.cpp)
target_link_libraries(RtkRover GnssHat)
