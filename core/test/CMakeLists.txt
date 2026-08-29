add_executable(
    wirespaces_core_tests
    header_test.cpp
    mailbox_test.cpp
    packet_test.cpp
    dispatch_test.cpp
    router_test.cpp
    local_domain_test.cpp
    hello_world_test.cpp
)

target_link_libraries(
    wirespaces_core_tests
    PRIVATE
        WireSpaces::runtime
        GTest::gtest_main
)

target_include_directories(
    wirespaces_core_tests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

wirespaces_configure_host_target(wirespaces_core_tests)

include(GoogleTest)
gtest_discover_tests(wirespaces_core_tests)
