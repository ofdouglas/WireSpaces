set(WIRESPACES_INCLUDE_ROOT ${PROJECT_SOURCE_DIR}/lib)

add_library(wirespaces_foundation INTERFACE)
add_library(WireSpaces::foundation ALIAS wirespaces_foundation)
target_include_directories(
    wirespaces_foundation
    INTERFACE ${WIRESPACES_INCLUDE_ROOT}
)

add_library(wirespaces_containers INTERFACE)
add_library(WireSpaces::containers ALIAS wirespaces_containers)
target_include_directories(
    wirespaces_containers
    INTERFACE ${WIRESPACES_INCLUDE_ROOT}
)
target_link_libraries(wirespaces_containers INTERFACE WireSpaces::foundation)

add_subdirectory(core)

add_library(wirespaces_crc INTERFACE)
add_library(WireSpaces::crc ALIAS wirespaces_crc)
target_include_directories(
    wirespaces_crc
    INTERFACE ${WIRESPACES_INCLUDE_ROOT}
)
target_link_libraries(wirespaces_crc INTERFACE WireSpaces::foundation)

add_library(wirespaces_hal INTERFACE)
add_library(WireSpaces::hal ALIAS wirespaces_hal)
target_include_directories(
    wirespaces_hal
    INTERFACE ${WIRESPACES_INCLUDE_ROOT}
)

add_library(wirespaces_runtime INTERFACE)
add_library(WireSpaces::runtime ALIAS wirespaces_runtime)
target_include_directories(
    wirespaces_runtime
    INTERFACE ${WIRESPACES_INCLUDE_ROOT}
)
target_link_libraries(wirespaces_runtime INTERFACE WireSpaces::core)

add_library(wirespaces_uart_hdlc INTERFACE)
add_library(WireSpaces::uart_hdlc ALIAS wirespaces_uart_hdlc)
target_include_directories(
    wirespaces_uart_hdlc
    INTERFACE ${WIRESPACES_INCLUDE_ROOT}
)
target_link_libraries(
    wirespaces_uart_hdlc
    INTERFACE WireSpaces::crc WireSpaces::foundation
)

add_library(
    wirespaces_bits
    STATIC
        transports/bits/bits.cpp
        transports/bits/receiver_engine.cpp
        transports/bits/codec.cpp
)
add_library(WireSpaces::bits ALIAS wirespaces_bits)
target_include_directories(
    wirespaces_bits
    PUBLIC ${WIRESPACES_INCLUDE_ROOT}
)
target_link_libraries(
    wirespaces_bits
    PUBLIC WireSpaces::core WireSpaces::foundation
)
wirespaces_configure_host_target(wirespaces_bits)

add_library(wirespaces_transports INTERFACE)
add_library(WireSpaces::transports ALIAS wirespaces_transports)
target_include_directories(
    wirespaces_transports
    INTERFACE ${WIRESPACES_INCLUDE_ROOT}
)
target_link_libraries(wirespaces_transports INTERFACE WireSpaces::bits)

add_library(wirespaces_services INTERFACE)
add_library(WireSpaces::services ALIAS wirespaces_services)
target_include_directories(
    wirespaces_services
    INTERFACE ${WIRESPACES_INCLUDE_ROOT}
)
target_link_libraries(
    wirespaces_services
    INTERFACE WireSpaces::runtime WireSpaces::hal
)

if(BUILD_TESTING AND WIRESPACES_BUILD_LIBRARY_TESTS)
    add_executable(
        wirespaces_containers_tests
        containers/test/queue_test.cpp
    )
    target_link_libraries(
        wirespaces_containers_tests
        PRIVATE WireSpaces::containers GTest::gtest_main
    )
    wirespaces_configure_host_target(wirespaces_containers_tests)

    include(GoogleTest)
    gtest_discover_tests(wirespaces_containers_tests)

    add_executable(
        wirespaces_services_tests
        services/test/polling_service_test.cpp
    )
    target_link_libraries(
        wirespaces_services_tests
        PRIVATE WireSpaces::services GTest::gtest_main
    )
    wirespaces_configure_host_target(wirespaces_services_tests)
    gtest_discover_tests(wirespaces_services_tests)

    add_executable(
        wirespaces_crc_tests
        crc/test/crc_test.cpp
    )
    target_link_libraries(
        wirespaces_crc_tests
        PRIVATE WireSpaces::crc GTest::gtest_main
    )
    wirespaces_configure_host_target(wirespaces_crc_tests)
    gtest_discover_tests(wirespaces_crc_tests)

    add_executable(
        wirespaces_uart_hdlc_tests
        links/uart_hdlc/test/hdlc_test.cpp
    )
    target_link_libraries(
        wirespaces_uart_hdlc_tests
        PRIVATE WireSpaces::uart_hdlc
    )
    wirespaces_configure_host_target(wirespaces_uart_hdlc_tests)
    add_test(NAME wirespaces_uart_hdlc_tests COMMAND wirespaces_uart_hdlc_tests)

    add_executable(
        wirespaces_bits_tests
        transports/bits/test/bits_test.cpp
    )
    target_include_directories(
        wirespaces_bits_tests
        PRIVATE transports/bits/test
    )
    target_link_libraries(
        wirespaces_bits_tests
        PRIVATE WireSpaces::bits GTest::gtest_main
    )
    wirespaces_configure_host_target(wirespaces_bits_tests)

    include(GoogleTest)
    gtest_discover_tests(wirespaces_bits_tests)

    add_executable(
        wirespaces_bits_narrow_tests
        transports/bits/test/receiver_engine_narrow_test.cpp
        transports/bits/receiver_engine.cpp
        transports/bits/codec.cpp
    )
    target_include_directories(
        wirespaces_bits_narrow_tests
        PRIVATE ${WIRESPACES_INCLUDE_ROOT}
    )
    target_compile_definitions(
        wirespaces_bits_narrow_tests
        PRIVATE WIRESPACES_BITS_RECEIVER_MAX_WINDOW_WIDTH=1
    )
    target_link_libraries(
        wirespaces_bits_narrow_tests
        PRIVATE WireSpaces::foundation GTest::gtest_main
    )
    wirespaces_configure_host_target(wirespaces_bits_narrow_tests)
    gtest_discover_tests(wirespaces_bits_narrow_tests)

endif()
