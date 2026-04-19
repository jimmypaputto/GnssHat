cmake_minimum_required(VERSION 3.12)

project(TimeBase)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(TimeBase src/TimeBase.cpp)
set_target_properties(TimeBase PROPERTIES OUTPUT_NAME time-base)
target_link_libraries(TimeBase GnssHat)
