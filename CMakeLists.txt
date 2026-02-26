# Jimmy Paputto 2025

cmake_minimum_required(VERSION 3.12)

project(GnssHat)
set(LIBGPIOD_VERSION 2)
add_compile_definitions(LIBGPIO_VERSION=${LIBGPIOD_VERSION})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    execute_process(
        COMMAND grep "Model" /proc/cpuinfo
        OUTPUT_VARIABLE RPI_MODEL
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    
    if(RPI_MODEL MATCHES "Raspberry Pi 5")
        add_definitions(-DRPI5)
        if(LIBGPIOD_VERSION MATCHES 2)
            add_compile_definitions(CHIP_NAME="/dev/gpiochip4")
            message(STATUS "Using LIBGPIOD >=2")
        else()
            add_compile_definitions(CHIP_NAME="gpiochip4")
            message(STATUS "Using LIBGPIOD < 2")
        endif()
        message(STATUS "Building for Raspberry Pi 5")
    elseif(RPI_MODEL MATCHES "Raspberry Pi 4")
        if(LIBGPIOD_VERSION MATCHES 2)
            add_compile_definitions(CHIP_NAME="/dev/gpiochip0")
            message(STATUS "Using LIBGPIOD >=2")
        else()
            add_compile_definitions(CHIP_NAME="gpiochip0")
            message(STATUS "Using LIBGPIOD < 2")
        endif()
        add_definitions(-DRPI4)
        message(STATUS "Building for Raspberry Pi 4")
    else()
        message(
            FATAL_ERROR
            "Unsupported platform: ${RPI_MODEL}. Raspberry Pi 4 or 5 is required."
        )
    endif()
else()
    message(
        FATAL_ERROR
        "Unsupported platform: ${CMAKE_SYSTEM_PROCESSOR}. Raspberry Pi 4 or 5 is required."
    )
endif()

include_directories(
    src
)

set(LIB_SOURCES
    src/common/GpioInterruptLine.cpp
    src/common/JPGuard.cpp
    src/common/Utils.cpp
    src/ublox/ubxmsg/IUbxMsg.cpp
    src/ublox/ubxmsg/UBX_CFG_VALGET.cpp
    src/ublox/Gnss.cpp
    src/ublox/GnssConfig.cpp
    src/ublox/NmeaForwarder.cpp
    src/ublox/Rtcm3Parser.cpp
    src/ublox/Rtcm3Store.cpp
    src/ublox/RTK.cpp
    src/ublox/Run.cpp
    src/ublox/SpiDriver.cpp
    src/ublox/Startup.cpp
    src/ublox/UartDriver.cpp
    src/ublox/Ublox.cpp
    src/ublox/UbloxConfigRegistry.cpp
    src/ublox/UbxCallbacks.cpp
    src/ublox/UbxFactory.cpp
    src/ublox/UbxParser.cpp
    src/GnssHat.cpp
    src/GnssHat_C.cpp
)

add_library(GnssHat SHARED ${LIB_SOURCES})

target_link_libraries(GnssHat gpiod pthread util)

install(TARGETS GnssHat
    LIBRARY DESTINATION /usr/local/lib
    ARCHIVE DESTINATION /usr/local/lib
)

install(
    FILES
        src/ublox/DilutionOverPrecision.hpp
        src/ublox/EDynamicModel.hpp
        src/ublox/EFixQuality.hpp
        src/ublox/EFixStatus.hpp
        src/ublox/EFixType.hpp
        src/ublox/ERtkMode.hpp
        src/ublox/Geofence.hpp
        src/ublox/Geofencing.hpp
        src/ublox/GnssConfig.hpp
        src/ublox/Navigation.hpp
        src/ublox/NmeaForwarder.hpp
        src/ublox/PositionVelocityTime.hpp
        src/ublox/RFBlock.hpp
        src/ublox/RTK.hpp
        src/ublox/RtkConfig.hpp
        src/ublox/SatelliteInfo.hpp
        src/ublox/TimepulsePinConfig.hpp
    DESTINATION
        /usr/local/include/jimmypaputto/ublox
)

install(
    FILES
        src/GnssHat.hpp
        src/GnssHat.h
    DESTINATION
        /usr/local/include/jimmypaputto
)
