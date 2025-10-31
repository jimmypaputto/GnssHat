cmake_minimum_required(VERSION 3.12)

project(GpsdIntegration)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Interactive example
add_executable(GpsdInteractive src/GpsdInteractive.cpp)
target_link_libraries(GpsdInteractive GnssHat)

# Daemon example
add_executable(jpgnss2gpsd-bridge src/Daemon.cpp)
target_link_libraries(jpgnss2gpsd-bridge GnssHat)

install(
    TARGETS jpgnss2gpsd-bridge
    RUNTIME DESTINATION /usr/local/bin/jimmypaputto
)

install(
    FILES README.md
    DESTINATION /usr/local/share/doc/jimmypaputto/jpgnss2gpsd-bridge
)

install(
    FILES res/jpgnss2gpsd-bridge.service
    DESTINATION /etc/systemd/system
)
