cmake_minimum_required(VERSION 3.12)

project(TimeBase)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(TimeBase src/TimeBase.cpp)
target_link_libraries(TimeBase GnssHat)
