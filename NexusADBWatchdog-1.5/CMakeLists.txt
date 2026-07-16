# Nexus ADB Watchdog Version 1.5
# Native ARM executable for Android 5.1.1 (RK3288 / ARMv7)
# Diagnostic / data-collection release (no automatic recovery).

cmake_minimum_required(VERSION 3.10.2)

project(NexusADBWatchdog15 C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Os -fPIC -ffunction-sections -fdata-sections")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Os")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -s")
endif()

include_directories(${CMAKE_SOURCE_DIR}/include)

set(WATCHDOG_SOURCES
    src/main.c
    src/watchdog.c
    src/logger.c
    src/network.c
    src/process.c
    src/status.c
    src/config.c
    src/util.c
)

add_executable(watchdog ${WATCHDOG_SOURCES})

set_target_properties(watchdog PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/release"
    OUTPUT_NAME "watchdog"
)

configure_file(
    ${CMAKE_SOURCE_DIR}/watchdog.conf
    ${CMAKE_SOURCE_DIR}/release/watchdog.conf
    COPYONLY
)

install(TARGETS watchdog RUNTIME DESTINATION bin)
