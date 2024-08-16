
cmake_minimum_required(VERSION 3.20)

project(mclient LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-DLINUX)
add_definitions(-DJUMBO_POOL)

# we converted C files into C++ files with minimal changes. We need to suppress some warnings. (TODO: fix them)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive")

option(PROMETHEUS "Enable Prometheus" ON)
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

add_executable(mclient src/minos/main.cpp)

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
        src/minos/mclient/femproxy.cpp
        src/minos/mclient/evbuilder.cpp
        src/minos/mclient/femarray.cpp
        src/minos/mclient/cmdfetcher.cpp
        src/minos/mclient/programflash.cpp
        src/minos/spiflash/hexreader.cpp
        src/minos/bufmgr/bufpool.cpp
        src/minos/feminos/frame.cpp
        src/minos/platforms/linux/os_al.cpp
)

if (PROMETHEUS)
    message(STATUS "Prometheus enabled")
    target_include_directories(mclient PRIVATE src/minos/prometheus)
    list(APPEND SOURCE_FILES src/minos/prometheus/prometheus.cpp)
    target_link_libraries(mclient PRIVATE prometheus-cpp::core prometheus-cpp::pull prometheus-cpp::push)
else ()
    message(STATUS "Prometheus disabled")
endif ()

target_sources(mclient PRIVATE ${SOURCE_FILES})
