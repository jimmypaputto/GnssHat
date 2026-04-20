cmake_minimum_required(VERSION 3.12)

project(PrintSatellites)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(PrintSatellitesBasic src/PrintSatellitesBasic.cpp)
set_target_properties(PrintSatellitesBasic PROPERTIES OUTPUT_NAME print-satellites-basic)
target_link_libraries(PrintSatellitesBasic GnssHat)

add_executable(PrintSatellitesAdvanced src/PrintSatellitesAdvanced.cpp)
set_target_properties(PrintSatellitesAdvanced PROPERTIES OUTPUT_NAME print-satellites-advanced)
target_link_libraries(PrintSatellitesAdvanced GnssHat)
