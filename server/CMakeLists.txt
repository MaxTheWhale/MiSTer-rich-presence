cmake_minimum_required(VERSION 3.18)

project(
    MiSTer-rich-presence
    VERSION 0.03
    DESCRIPTION "Discord Rich Presence support for MiSTer"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)
set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)

add_compile_options(-Wall -Wextra -pedantic -Werror)

add_executable(mister-status)
add_subdirectory(src)
