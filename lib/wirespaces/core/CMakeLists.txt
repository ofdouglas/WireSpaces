add_library(
    wirespaces_core
    STATIC
        dispatch.cpp
        domain.cpp
        host.cpp
        mailbox.cpp
        packet.cpp
        router.cpp
)
add_library(WireSpaces::core ALIAS wirespaces_core)

target_compile_features(wirespaces_core PUBLIC cxx_std_17)

target_include_directories(
    wirespaces_core
    PUBLIC ${PROJECT_SOURCE_DIR}/lib
)

target_link_libraries(
    wirespaces_core
    PUBLIC WireSpaces::containers WireSpaces::foundation
)

if(MSVC)
    target_compile_options(wirespaces_core PRIVATE /W4 /WX /permissive-)
else()
    target_compile_options(
        wirespaces_core
        PRIVATE -Wall -Wextra -Wpedantic -Werror
    )
endif()
