cmake_minimum_required(VERSION 3.20)

project(mclient LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-DLINUX)
add_definitions(-DJUMBO_POOL)

# we converted C files into C++ files with minimal changes. We need to suppress
# some warnings. TODO: fix them
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive")

find_package(ROOT REQUIRED COMPONENTS RIO Tree)

include(FetchContent)

FetchContent_Declare(
    Prometheus
    GIT_REPOSITORY https://github.com/jupp0r/prometheus-cpp.git
    GIT_TAG v1.2.4)
FetchContent_MakeAvailable(Prometheus)

FetchContent_Declare(
    CLI11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.4.2)
FetchContent_MakeAvailable(CLI11)

add_executable(mclient src/minos/main.cpp)

target_include_directories(
    mclient
    PRIVATE ${ROOT_INCLUDE_DIRS}
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
            src/minos/prometheus
            src/minos/root)

file(
    GLOB_RECURSE
    SOURCE_FILES
    src/minos/mclient/femproxy.cpp
    src/minos/mclient/evbuilder.cpp
    src/minos/mclient/femarray.cpp
    src/minos/mclient/cmdfetcher.cpp
    src/minos/mclient/programflash.cpp
    src/minos/spiflash/hexreader.cpp
    src/minos/bufmgr/bufpool.cpp
    src/minos/feminos/frame.cpp
    src/minos/platforms/linux/os_al.cpp
    src/minos/prometheus/prometheus.cpp
    src/minos/root/storage.cpp
    src/minos/root/graph.cpp)

target_link_libraries(
    mclient PRIVATE CLI11::CLI11 prometheus-cpp::core prometheus-cpp::pull
                    prometheus-cpp::push ${ROOT_LIBRARIES})

target_sources(mclient PRIVATE ${SOURCE_FILES})
