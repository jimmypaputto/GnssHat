cmake_minimum_required(VERSION 3.12)

project(PrintNavigation)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(PrintNavigationAdvanced src/PrintNavigationAdvanced.cpp)
target_link_libraries(PrintNavigationAdvanced GnssHat)

add_executable(PrintNavigationBasic src/PrintNavigationBasic.cpp)
target_link_libraries(PrintNavigationBasic GnssHat)
