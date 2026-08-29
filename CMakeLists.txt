cmake_minimum_required(VERSION 3.20)

project(
    WireSpaces
    VERSION 0.1.0
    DESCRIPTION "Network and messaging stack for hierarchical embedded systems"
    LANGUAGES CXX
)

include(CTest)

function(wirespaces_configure_host_target target_name)
    target_compile_features(${target_name} PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /WX /permissive-)
    else()
        target_compile_options(
            ${target_name}
            PRIVATE -Wall -Wextra -Wpedantic -Werror
        )
    endif()
endfunction()

option(WIRESPACES_BUILD_CORE_TESTS "Build core tests" ON)
option(WIRESPACES_BUILD_LIBRARY_TESTS "Build reusable library tests" ON)
option(WIRESPACES_BUILD_SIMULATOR "Build the Linux-hosted simulator" ON)

if(BUILD_TESTING AND (WIRESPACES_BUILD_CORE_TESTS OR WIRESPACES_BUILD_LIBRARY_TESTS OR WIRESPACES_BUILD_SIMULATOR))
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.18.0
        GIT_SHALLOW TRUE
    )
    set(INSTALL_GTEST OFF CACHE BOOL "Do not install GoogleTest" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "Do not build GoogleMock" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

add_subdirectory(lib/wirespaces)

if(BUILD_TESTING AND WIRESPACES_BUILD_CORE_TESTS)
    add_subdirectory(lib/wirespaces/core/test)
endif()

if(WIRESPACES_BUILD_SIMULATOR)
    add_subdirectory(sim)
endif()
