set(WIRESPACES_INCLUDE_ROOT ${PROJECT_SOURCE_DIR}/lib)

add_library(wirespaces_foundation INTERFACE)
add_library(WireSpaces::foundation ALIAS wirespaces_foundation)
target_include_directories(
    wirespaces_foundation
    INTERFACE ${WIRESPACES_INCLUDE_ROOT}
)

add_subdirectory(core)

add_library(wirespaces_crc INTERFACE)
add_library(WireSpaces::crc ALIAS wirespaces_crc)
target_include_directories(
    wirespaces_crc
    INTERFACE ${WIRESPACES_INCLUDE_ROOT}
)
target_link_libraries(wirespaces_crc INTERFACE WireSpaces::foundation)

add_library(wirespaces_containers INTERFACE)
add_library(WireSpaces::containers ALIAS wirespaces_containers)
target_include_directories(
    wirespaces_containers
    INTERFACE ${WIRESPACES_INCLUDE_ROOT}
)
target_link_libraries(wirespaces_containers INTERFACE WireSpaces::foundation)

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

add_library(wirespaces_transports INTERFACE)
add_library(WireSpaces::transports ALIAS wirespaces_transports)
target_include_directories(
    wirespaces_transports
    INTERFACE ${WIRESPACES_INCLUDE_ROOT}
)

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
        wirespaces_uart_hdlc_tests
        links/uart_hdlc/test/hdlc_test.cpp
    )
    target_link_libraries(
        wirespaces_uart_hdlc_tests
        PRIVATE WireSpaces::uart_hdlc
    )
    wirespaces_configure_host_target(wirespaces_uart_hdlc_tests)
    add_test(NAME wirespaces_uart_hdlc_tests COMMAND wirespaces_uart_hdlc_tests)
endif()
