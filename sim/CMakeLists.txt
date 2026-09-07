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
        command_line.cpp
        ecu_logger.cpp
        ecu_process.cpp
        signal_shutdown.cpp
        system_monotonic_clock.cpp
)
add_library(WireSpaces::sim ALIAS wirespaces_sim)

target_include_directories(
    wirespaces_sim
    PUBLIC ${PROJECT_SOURCE_DIR}
)
target_link_libraries(wirespaces_sim PUBLIC Threads::Threads)
wirespaces_configure_sim_target(wirespaces_sim)

add_executable(ws_sim_counter_ecu examples/counter_ecu.cpp)
target_link_libraries(ws_sim_counter_ecu PRIVATE WireSpaces::sim)
wirespaces_configure_sim_target(ws_sim_counter_ecu)

if(BUILD_TESTING)
    add_executable(wirespaces_sim_tests test/simulator_tests.cpp)
    target_link_libraries(
        wirespaces_sim_tests
        PRIVATE WireSpaces::sim GTest::gtest_main
    )
    wirespaces_configure_host_target(wirespaces_sim_tests)
    include(GoogleTest)
    gtest_discover_tests(wirespaces_sim_tests)
endif()
