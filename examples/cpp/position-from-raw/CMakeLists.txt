cmake_minimum_required(VERSION 3.12)

project(PositionFromRaw)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(PositionFromRaw src/PositionFromRaw.cpp)
set_target_properties(PositionFromRaw PROPERTIES OUTPUT_NAME position-from-raw)
target_link_libraries(PositionFromRaw GnssHat)
