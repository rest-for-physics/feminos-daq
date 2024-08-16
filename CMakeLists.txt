
cmake_minimum_required(VERSION 3.20)

project(mclient)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-DLINUX)
add_definitions(-DJUMBO_POOL)

option(PROMETHEUS "Enable Prometheus" OFF)
# option(TEST "Enable test" OFF)

include(FetchContent)

if (PROMETHEUS)
    FetchContent_Declare(
            Prometheus
            GIT_REPOSITORY https://github.com/jupp0r/prometheus-cpp.git
            GIT_TAG v1.2.4
    )
    FetchContent_MakeAvailable(Prometheus)
endif ()

add_executable(mclient src/minos/mclient/mclientUDP.c)

target_include_directories(mclient PRIVATE
        src/minos/bootloader
        src/minos/bufmgr
        src/minos/cmdi
        src/minos/common
        src/minos/feminos
        src/minos/mars
        src/minos/mclient
        src/minos/network
        src/minos/platforms
        src/minos/platforms/linux
        src/minos/spiflash
        src/minos/tcm
        src/minos/util
        src/minos/util/linux
)

file(GLOB_RECURSE SOURCE_FILES
        src/minos/mclient/femproxy.c
        src/minos/mclient/evbuilder.c
        src/minos/mclient/femarray.c
        src/minos/mclient/cmdfetcher.c
        src/minos/mclient/programflash.c
        src/minos/spiflash/hexreader.c
        src/minos/bufmgr/bufpool.c
        src/minos/feminos/frame.c
        src/minos/platforms/linux/os_al.c
)

if (PROMETHEUS)
    message(STATUS "Prometheus enabled")
    target_include_directories(mclient PRIVATE src/minos/prometheus)
    list(APPEND SOURCE_FILES src/minos/prometheus/prometheus.c)
    target_link_libraries(mclient PRIVATE prometheus-cpp::core prometheus-cpp::pull prometheus-cpp::push)
endif ()

target_sources(mclient PRIVATE ${SOURCE_FILES})
