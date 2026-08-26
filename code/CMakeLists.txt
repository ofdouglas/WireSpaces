cmake_minimum_required(VERSION 3.20)

project(
    WireSpaces
    VERSION 0.1.0
    DESCRIPTION "Network and messaging stack for hierarchical embedded systems"
    LANGUAGES C CXX
)

include(CTest)

function(wirespaces_configure_host_target target_name)
    target_compile_features(${target_name} PRIVATE cxx_std_17)

    if(MSVC)
        target_compile_options(
            ${target_name}
            PRIVATE
                /W4
                /WX
                /permissive-
        )
    else()
        target_compile_options(
            ${target_name}
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Werror
        )
    endif()
endfunction()

add_library(
    wirespaces_core
    STATIC
        src/dispatch.c
        src/header.c
        src/host.c
        src/local_domain.c
        src/mailbox.c
        src/packet.c
        src/router.c
)

target_include_directories(
    wirespaces_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

if(MSVC)
    target_compile_options(wirespaces_core PRIVATE /W4 /WX)
else()
    target_compile_options(
        wirespaces_core
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
    )
endif()

option(WIRESPACES_BUILD_CORE_TESTS "Build WireSpaces core GoogleTest targets" ON)
option(WIRESPACES_BUILD_SIMULATOR "Build the Linux-hosted WireSpaces simulator" ON)

if(WIRESPACES_BUILD_CORE_TESTS AND BUILD_TESTING)
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

    add_executable(wirespaces_core_tests test/hello_world_test.cpp)
    target_link_libraries(
        wirespaces_core_tests
        PRIVATE
            wirespaces_core
            GTest::gtest_main
    )
    target_include_directories(
        wirespaces_core_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/test
    )
    wirespaces_configure_host_target(wirespaces_core_tests)

    include(GoogleTest)
    gtest_discover_tests(wirespaces_core_tests)
endif()

if(WIRESPACES_BUILD_SIMULATOR)
    add_subdirectory(sim)
endif()
