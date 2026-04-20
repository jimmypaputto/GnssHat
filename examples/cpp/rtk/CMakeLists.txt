cmake_minimum_required(VERSION 3.12)

project(RTK)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(RtkBase src/RtkBase.cpp)
set_target_properties(RtkBase PROPERTIES OUTPUT_NAME rtk-base)
target_link_libraries(RtkBase GnssHat)

add_executable(RtkRover src/RtkRover.cpp)
set_target_properties(RtkRover PROPERTIES OUTPUT_NAME rtk-rover)
target_link_libraries(RtkRover GnssHat)

add_executable(RtkServer src/RtkServer.cpp)
set_target_properties(RtkServer PROPERTIES OUTPUT_NAME rtk-server)
target_link_libraries(RtkServer GnssHat)
