
cmake_minimum_required(VERSION 3.20)
project(mclient)

set(CMAKE_CXX_STANDARD 20)

add_definitions(-DLINUX)
add_definitions(-DJUMBO_POOL)

file(GLOB_RECURSE INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/*/*)

add_executable(mclient ${CMAKE_CURRENT_SOURCE_DIR}/mclient/mclientUDP.c)

target_include_directories(mclient PRIVATE
        bootloader
        bufmgr
        cmdi
        common
        feminos
        mars
        mclient
        network
        platforms
        platforms/linux
        spiflash
        tcm
        util
        util/linux
)

file(GLOB_RECURSE SOURCE_FILES
        mclient/mclientUDP.c
        mclient/femproxy.c
        mclient/evbuilder.c
        mclient/femarray.c
        mclient/cmdfetcher.c
        mclient/programflash.c
        spiflash/hexreader.c
        bufmgr/bufpool.c
        feminos/frame.c
        platforms/linux/os_al.c
)

target_sources(mclient PRIVATE ${SOURCE_FILES})
