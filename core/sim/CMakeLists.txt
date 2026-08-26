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

function(wirespaces_configure_sim_target target_name)
    wirespaces_configure_host_target(${target_name})

    if(MSVC)
        target_compile_options(${target_name} PRIVATE /GR- /EHs-c-)
    else()
        target_compile_options(${target_name} PRIVATE -fno-exceptions -fno-rtti)
    endif()
endfunction()

find_package(Threads REQUIRED)

add_library(
    wirespaces_sim
    STATIC
        src/command_line.cpp
        src/ecu_logger.cpp
        src/ecu_process.cpp
        src/signal_shutdown.cpp
        src/system_monotonic_clock.cpp
)

target_include_directories(
    wirespaces_sim
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(wirespaces_sim PUBLIC Threads::Threads)
wirespaces_configure_sim_target(wirespaces_sim)

add_executable(ws_sim_counter_ecu examples/counter_ecu.cpp)
target_link_libraries(ws_sim_counter_ecu PRIVATE wirespaces_sim)
wirespaces_configure_sim_target(ws_sim_counter_ecu)

if(BUILD_TESTING)
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

    add_executable(wirespaces_sim_tests test/simulator_tests.cpp)
    target_link_libraries(
        wirespaces_sim_tests
        PRIVATE
            wirespaces_sim
            GTest::gtest_main
    )
    wirespaces_configure_host_target(wirespaces_sim_tests)

    include(GoogleTest)
    gtest_discover_tests(wirespaces_sim_tests)
endif()
