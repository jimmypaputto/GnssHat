# Jimmy Paputto 2026

cmake_minimum_required(VERSION 3.12)

project(GnssHatTests)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

set(LIBGPIOD_VERSION 2)
add_compile_definitions(LIBGPIO_VERSION=${LIBGPIOD_VERSION})

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    execute_process(
        COMMAND grep "Model" /proc/cpuinfo
        OUTPUT_VARIABLE RPI_MODEL
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(RPI_MODEL MATCHES "Raspberry Pi 5")
        add_definitions(-DRPI5)
        add_compile_definitions(CHIP_NAME="/dev/gpiochip4")
    elseif(RPI_MODEL MATCHES "Raspberry Pi 4")
        add_definitions(-DRPI4)
        add_compile_definitions(CHIP_NAME="/dev/gpiochip0")
    endif()
endif()

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src)

include_directories(${SRC_DIR})

find_package(GTest REQUIRED)

set(LIB_SOURCES
    ${SRC_DIR}/common/JPGuard.cpp
    ${SRC_DIR}/common/Utils.cpp
    ${SRC_DIR}/common/GpioInterruptLine.cpp
    ${SRC_DIR}/ublox/ubxmsg/IUbxMsg.cpp
    ${SRC_DIR}/ublox/ubxmsg/UBX_CFG_VALGET.cpp
    ${SRC_DIR}/ublox/Gnss.cpp
    ${SRC_DIR}/ublox/GnssConfig.cpp
    ${SRC_DIR}/ublox/NmeaForwarder.cpp
    ${SRC_DIR}/ublox/Rtcm3Parser.cpp
    ${SRC_DIR}/ublox/Rtcm3Store.cpp
    ${SRC_DIR}/ublox/RTK.cpp
    ${SRC_DIR}/ublox/Run.cpp
    ${SRC_DIR}/ublox/SpiDriver.cpp
    ${SRC_DIR}/ublox/Startup.cpp
    ${SRC_DIR}/ublox/UartDriver.cpp
    ${SRC_DIR}/ublox/Ublox.cpp
    ${SRC_DIR}/ublox/UbloxConfigRegistry.cpp
    ${SRC_DIR}/ublox/UbxCallbacks.cpp
    ${SRC_DIR}/ublox/UbxFactory.cpp
    ${SRC_DIR}/ublox/UbxParser.cpp
)

add_executable(GnssHatTests
    TestUtils.cpp
    TestUbxChecksum.cpp
    TestGnssConfig.cpp
    TestUbxMessages.cpp
    TestRtcm3.cpp
    TestUbxClassMsgId.cpp
    ${LIB_SOURCES}
)

target_link_libraries(GnssHatTests GTest::GTest GTest::Main gpiod pthread util)

enable_testing()
add_test(NAME GnssHatTests COMMAND GnssHatTests)
