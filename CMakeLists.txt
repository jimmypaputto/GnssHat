# Jimmy Paputto 2025

cmake_minimum_required(VERSION 3.12)

project(GnssHat)

option(BUILD_PYTHON "Build Python bindings" OFF)
option(BUILD_EXAMPLES "Build C and C++ examples" OFF)

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
    src/ublox/BaseConfig.cpp
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

if(BUILD_EXAMPLES)
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include)
    execute_process(COMMAND ln -snf ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/include/jimmypaputto)
    target_include_directories(GnssHat PUBLIC $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>)

    # C examples
    add_subdirectory(examples/C/Geofencing)
    add_subdirectory(examples/C/HotStart)
    add_subdirectory(examples/C/JammingDetector)
    add_subdirectory(examples/C/PrintNavigation)
    add_subdirectory(examples/C/PrintSatellites)
    add_subdirectory(examples/C/RTK)
    add_subdirectory(examples/C/TimeBase)
    add_subdirectory(examples/C/TimeMark)
    add_subdirectory(examples/C/TimepulseInterrupt)

    # C++ examples
    add_subdirectory(examples/CPP/Geofencing)
    add_subdirectory(examples/CPP/HotStart)
    add_subdirectory(examples/CPP/JammingDetector)
    add_subdirectory(examples/CPP/PrintNavigation)
    add_subdirectory(examples/CPP/PrintSatellites)
    add_subdirectory(examples/CPP/RTK)
    add_subdirectory(examples/CPP/TimeBase)
    add_subdirectory(examples/CPP/TimeMark)
    add_subdirectory(examples/CPP/TimepulseInterrupt)

    # Create BinariesSymlinks directory with symlinks to C++ example binaries
    set(SYMLINK_DIR ${CMAKE_SOURCE_DIR}/examples/BinariesSymlinks)
    file(MAKE_DIRECTORY ${SYMLINK_DIR})

    set(CPP_TARGETS
        Geofencing
        HotStart
        JammingDetector
        PrintNavigationAdvanced
        PrintNavigationBasic
        PrintSatellitesBasic
        PrintSatellitesAdvanced
        RtkBase
        RtkRover
        TimeBase
        TimeMark
        TimepulseInterrupt
    )

    set(SYMLINK_COMMANDS)
    foreach(TARGET_NAME ${CPP_TARGETS})
        list(APPEND SYMLINK_COMMANDS
            COMMAND ln -snf $<TARGET_FILE:${TARGET_NAME}> ${SYMLINK_DIR}/${TARGET_NAME}
        )
    endforeach()

    add_custom_target(BinariesSymlinks ALL
        ${SYMLINK_COMMANDS}
        DEPENDS ${CPP_TARGETS}
        COMMENT "Creating symlinks in examples/BinariesSymlinks/"
    )

    set(SYMLINK_FILES)
    foreach(TARGET_NAME ${CPP_TARGETS})
        list(APPEND SYMLINK_FILES ${SYMLINK_DIR}/${TARGET_NAME})
    endforeach()
    set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_CLEAN_FILES ${SYMLINK_FILES})
endif()

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
        src/ublox/BaseConfig.hpp
        src/ublox/RtkConfig.hpp
        src/ublox/TimingConfig.hpp
        src/ublox/SatelliteInfo.hpp
        src/ublox/TimeMark.hpp
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

if(BUILD_PYTHON)
    add_subdirectory(python)
endif()
