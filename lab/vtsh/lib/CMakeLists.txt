add_library(
    libvtsh
    STATIC
    vtsh.c
    ema_sort.c
)

target_include_directories(
    libvtsh
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)

target_compile_options(libvtsh PRIVATE -Wall -Wextra -Wpedantic)
