add_library(
    wirespaces_core
    STATIC
        dispatch.c
        header.c
        host.c
        local_domain.c
        mailbox.c
        packet.c
        router.c
)
add_library(WireSpaces::core ALIAS wirespaces_core)

target_include_directories(
    wirespaces_core
    PUBLIC ${PROJECT_SOURCE_DIR}
)

if(MSVC)
    target_compile_options(wirespaces_core PRIVATE /W4 /WX)
else()
    target_compile_options(
        wirespaces_core
        PRIVATE -Wall -Wextra -Wpedantic -Werror
    )
endif()
