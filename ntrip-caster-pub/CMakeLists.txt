cmake_minimum_required(VERSION 3.16)
project(ntrip-caster-pub
    VERSION 1.0.0
    DESCRIPTION "Standalone single-mountpoint NTRIP v2.0 caster"
    LANGUAGES CXX)

# ── Options ────────────────────────────────────────────────────────────
option(NTRIP_CASTER_TLS  "Build with OpenSSL TLS support"        OFF)
option(NTRIP_CASTER_TESTS "Build internal smoke tests"            OFF)
option(NTRIP_CASTER_STATIC "Build a fully static binary"          OFF)

# ── Compiler ───────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

# ── Dependencies ───────────────────────────────────────────────────────
find_package(Threads REQUIRED)

set(CASTER_SOURCES
    src/NtripCaster.cpp
)

set(CASTER_HEADERS
    src/NtripCaster.hpp
    src/NtripLog.hpp
    src/NtripStats.hpp
    src/NtripTls.hpp
    src/Base64.hpp
    src/RtcmArp.hpp
)

# ── Library ────────────────────────────────────────────────────────────
add_library(ntripcaster STATIC ${CASTER_SOURCES} ${CASTER_HEADERS})
target_include_directories(ntripcaster PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>)
target_link_libraries(ntripcaster PUBLIC Threads::Threads)

if(NTRIP_CASTER_TLS)
    find_package(OpenSSL REQUIRED)
    target_compile_definitions(ntripcaster PUBLIC NTRIP_CASTER_HAS_TLS)
    target_link_libraries(ntripcaster PUBLIC OpenSSL::SSL OpenSSL::Crypto)
    message(STATUS "ntrip-caster-pub: TLS support enabled (OpenSSL ${OPENSSL_VERSION})")
else()
    message(STATUS "ntrip-caster-pub: TLS support DISABLED")
endif()

# ── Executable ─────────────────────────────────────────────────────────
add_executable(ntrip-caster app/ntrip-caster.cpp)
target_link_libraries(ntrip-caster PRIVATE ntripcaster)

if(NTRIP_CASTER_STATIC)
    set_target_properties(ntrip-caster PROPERTIES
        LINK_FLAGS "-static")
endif()

# ── Install ────────────────────────────────────────────────────────────
include(GNUInstallDirs)

install(TARGETS ntrip-caster
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES systemd/ntrip-caster.service
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/system
        OPTIONAL)

# ── Tests ──────────────────────────────────────────────────────────────
if(NTRIP_CASTER_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "ntrip-caster-pub configured: prefix=${CMAKE_INSTALL_PREFIX}")
