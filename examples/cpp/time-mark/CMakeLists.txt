cmake_minimum_required(VERSION 3.12)

project(TimeMark)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(
    SOURCES
    src/TimeMark.cpp
)

add_executable(TimeMark ${SOURCES})
set_target_properties(TimeMark PROPERTIES OUTPUT_NAME time-mark)

target_link_libraries(
    TimeMark
    GnssHat
)
