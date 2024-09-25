cmake_minimum_required(VERSION 3.17)

project(
    feminos-daq
    LANGUAGES CXX
    VERSION 2.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# We converted C files into C++ files with minimal changes. We need to suppress
# some warnings. TODO: fix them
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive")

find_package(Threads REQUIRED)
find_package(ROOT REQUIRED COMPONENTS RIO Tree)

include(FetchContent)

option(ENABLE_TESTING "Enable testing of the Prometheus library" OFF)

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

add_executable(${PROJECT_NAME} src/main.cpp)

target_compile_definitions(
    ${PROJECT_NAME}
    PRIVATE FEMINOS_DAQ_VERSION_MAJOR=${feminos-daq_VERSION_MAJOR}
            FEMINOS_DAQ_VERSION_MINOR=${feminos-daq_VERSION_MINOR}
            FEMINOS_DAQ_VERSION_PATCH=${feminos-daq_VERSION_PATCH}
            LINUX
            JUMBO_POOL)

target_compile_definitions(${PROJECT_NAME}
                           PRIVATE FEMINOS_DAQ_VERSION="${feminos-daq_VERSION}")

target_include_directories(
    ${PROJECT_NAME}
    PRIVATE ${ROOT_INCLUDE_DIRS}
            src/bufmgr
            src/feminos
            src/mclient
            src/platforms
            src/platforms/linux
            src/util
            src/util/linux
            src/prometheus
            src/root)

file(
    GLOB_RECURSE
    SOURCE_FILES
    src/mclient/femproxy.cpp
    src/mclient/evbuilder.cpp
    src/mclient/femarray.cpp
    src/mclient/cmdfetcher.cpp
    src/bufmgr/bufpool.cpp
    src/feminos/frame.cpp
    src/platforms/linux/os_al.cpp
    src/prometheus/prometheus.cpp
    src/root/storage.cpp)

target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE CLI11::CLI11 prometheus-cpp::core prometheus-cpp::pull
            prometheus-cpp::push ${ROOT_LIBRARIES})

target_sources(${PROJECT_NAME} PRIVATE ${SOURCE_FILES})

# Install the binary and the viewer script
install(TARGETS ${PROJECT_NAME} DESTINATION bin)

install(
    FILES viewer/feminos-viewer.py
    DESTINATION bin
    RENAME feminos-viewer)
# make sure it has execute permissions (installing with sudo may remove them)
install(
    CODE "execute_process(COMMAND chmod +x ${CMAKE_INSTALL_PREFIX}/bin/feminos-viewer)"
)

install(FILES scripts/feminos-daq-sync.sh DESTINATION bin)
install(
    CODE "execute_process(COMMAND chmod +x ${CMAKE_INSTALL_PREFIX}/bin/feminos-daq-sync.sh)"
)

# Uninstall
add_custom_target(uninstall COMMAND ${CMAKE_COMMAND} -P
                                    ${CMAKE_SOURCE_DIR}/cmake/uninstall.cmake)
