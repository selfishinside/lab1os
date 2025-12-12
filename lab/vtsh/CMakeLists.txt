cmake_minimum_required(VERSION 3.12)

project(
    vtsh
    VERSION 0.1
    DESCRIPTION "VT Shell Project (proc-vfork, shell-seq, cpu-mat-mul, ema-sort-int)"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra -Werror -fsanitize=address)
add_link_options(-fsanitize=address)

add_subdirectory(lib)
add_subdirectory(bin)
