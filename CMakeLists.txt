
cmake_minimum_required(VERSION 3.20)

project(mclient)

set(CMAKE_CXX_STANDARD 20)

add_definitions(-DLINUX)
add_definitions(-DJUMBO_POOL)

file(GLOB_RECURSE INCLUDE_DIRECTORIES src/minos/*/*)

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
        src/minos/mclient/mclientUDP.c
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

target_sources(mclient PRIVATE ${SOURCE_FILES})
