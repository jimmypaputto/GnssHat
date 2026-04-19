# Jimmy Paputto 2025

cmake_minimum_required(VERSION 3.12)

project(GnssHat VERSION 1.0.0)

option(BUILD_PYTHON "Build Python bindings" OFF)
option(BUILD_EXAMPLES "Build C and C++ examples" OFF)
option(NTRIP_TLS_SUPPORT "Enable TLS for NTRIP connections (requires OpenSSL)" OFF)

set(LIBGPIOD_VERSION 2)
add_compile_definitions(LIBGPIOD_VERSION=${LIBGPIOD_VERSION})

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
    src/ntrip/NtripCaster.cpp
    src/ntrip/NtripClient.cpp
    src/ntrip/NtripServer.cpp
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

set_target_properties(GnssHat PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    OUTPUT_NAME gnsshat
)

target_link_libraries(GnssHat gpiod pthread util)

if(NTRIP_TLS_SUPPORT)
    find_package(OpenSSL REQUIRED)
    target_compile_definitions(GnssHat PRIVATE GNSSHAT_HAS_TLS)
    target_link_libraries(GnssHat OpenSSL::SSL OpenSSL::Crypto)
    message(STATUS "NTRIP TLS support enabled")
endif()

if(BUILD_EXAMPLES)
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include)
    execute_process(COMMAND ln -snf ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/include/jimmypaputto)
    target_include_directories(GnssHat PUBLIC $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>)

    # C examples
    add_subdirectory(examples/c/geofencing)
    add_subdirectory(examples/c/hot-start)
    add_subdirectory(examples/c/jamming-detector)
    add_subdirectory(examples/c/print-navigation)
    add_subdirectory(examples/c/print-satellites)
    add_subdirectory(examples/c/rtk)
    add_subdirectory(examples/c/time-base)
    add_subdirectory(examples/c/time-mark)
    add_subdirectory(examples/c/timepulse-interrupt)

    # C++ examples
    add_subdirectory(examples/cpp/geofencing)
    add_subdirectory(examples/cpp/hot-start)
    add_subdirectory(examples/cpp/jamming-detector)
    add_subdirectory(examples/cpp/print-navigation)
    add_subdirectory(examples/cpp/print-satellites)
    add_subdirectory(examples/cpp/rtk)
    add_subdirectory(examples/cpp/time-base)
    add_subdirectory(examples/cpp/time-mark)
    add_subdirectory(examples/cpp/timepulse-interrupt)

    # Create bin directory with symlinks to C++ example binaries
    set(SYMLINK_DIR ${CMAKE_SOURCE_DIR}/examples/bin)
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

    # Map CMake target names to output binary names for symlinks
    set(CPP_SYMLINK_NAMES
        geofencing
        hot-start
        jamming-detector
        print-navigation-advanced
        print-navigation-basic
        print-satellites-basic
        print-satellites-advanced
        rtk-base
        rtk-rover
        time-base
        time-mark
        timepulse-interrupt
    )

    set(SYMLINK_COMMANDS)
    list(LENGTH CPP_TARGETS _len)
    math(EXPR _last "${_len} - 1")
    foreach(_i RANGE ${_last})
        list(GET CPP_TARGETS ${_i} _target)
        list(GET CPP_SYMLINK_NAMES ${_i} _name)
        list(APPEND SYMLINK_COMMANDS
            COMMAND ln -snf $<TARGET_FILE:${_target}> ${SYMLINK_DIR}/${_name}
        )
    endforeach()

    add_custom_target(BinariesSymlinks ALL
        ${SYMLINK_COMMANDS}
        DEPENDS ${CPP_TARGETS}
        COMMENT "Creating symlinks in examples/bin/"
    )

    set(SYMLINK_FILES)
    foreach(_name ${CPP_SYMLINK_NAMES})
        list(APPEND SYMLINK_FILES ${SYMLINK_DIR}/${_name})
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
        src/ublox/PositionVelocityTime.hpp
        src/ublox/RFBlock.hpp
        src/ublox/RTK.hpp
        src/ublox/BaseConfig.hpp
        src/ublox/RtkConfig.hpp
        src/ublox/TimingConfig.hpp
        src/ublox/SatelliteInfo.hpp
        src/ublox/TimeMark.hpp
        src/ublox/TimepulsePinConfig.hpp
        src/ublox/RFBlockSpectrumData.hpp
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

install(
    FILES
        src/ntrip/NtripCaster.hpp
        src/ntrip/NtripClient.hpp
        src/ntrip/NtripServer.hpp
        src/ntrip/NtripTls.hpp
    DESTINATION
        /usr/local/include/jimmypaputto/ntrip
)

if(BUILD_PYTHON)
    add_subdirectory(python)
endif()
