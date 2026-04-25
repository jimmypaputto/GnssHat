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
    src/CasterConfig.hpp
    src/HttpStatusServer.hpp
)

# ── Library ────────────────────────────────────────────────────────────
add_library(ntripcaster STATIC ${CASTER_SOURCES} ${CASTER_HEADERS})
target_include_directories(ntripcaster PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party>)
target_link_libraries(ntripcaster PUBLIC Threads::Threads)
target_compile_definitions(ntripcaster PUBLIC
    NTRIP_CASTER_INSTALLED_WEB_ROOT="${CMAKE_INSTALL_FULL_DATADIR}/ntrip-caster/web")

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

install(DIRECTORY web/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/ntrip-caster/web)

# Config lives under /etc/ntrip-caster regardless of CMAKE_INSTALL_PREFIX,
# so the systemd unit's hard-coded path stays valid.  The .example file is
# always installed; the live ntrip-caster.toml is only dropped if no
# existing config is present (never overwrites operator edits).
set(NTRIP_CASTER_CONF_DIR "/etc/ntrip-caster"
    CACHE PATH "Absolute path where ntrip-caster.toml is installed")

install(FILES ntrip-caster.toml.example
        DESTINATION ${NTRIP_CASTER_CONF_DIR})

install(CODE "
    set(_cfg \"\$ENV{DESTDIR}${NTRIP_CASTER_CONF_DIR}/ntrip-caster.toml\")
    if(NOT EXISTS \"\${_cfg}\")
        message(STATUS \"Installing default config: \${_cfg}\")
        file(INSTALL
             DESTINATION \"${NTRIP_CASTER_CONF_DIR}\"
             TYPE FILE
             RENAME ntrip-caster.toml
             FILES \"${CMAKE_CURRENT_SOURCE_DIR}/ntrip-caster.toml.example\")
    else()
        message(STATUS \"Keeping existing config: \${_cfg}\")
    endif()
")

install(FILES systemd/ntrip-caster.service
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/system
        OPTIONAL)

# ── Tests ──────────────────────────────────────────────────────────────
if(NTRIP_CASTER_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "ntrip-caster-pub configured: prefix=${CMAKE_INSTALL_PREFIX}")
