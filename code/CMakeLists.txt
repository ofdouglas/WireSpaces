cmake_minimum_required(VERSION 3.20)

project(
    WireSpaces
    VERSION 0.1.0
    DESCRIPTION "Network and messaging stack for hierarchical embedded systems"
    LANGUAGES CXX
)

include(CTest)

option(WIRESPACES_BUILD_SIMULATOR "Build the Linux-hosted WireSpaces simulator" ON)

if(WIRESPACES_BUILD_SIMULATOR)
    add_subdirectory(sim)
endif()
