cmake_minimum_required(VERSION 3.23)

project(
    MiSTer-rich-presence
    VERSION 0.03
    DESCRIPTION "Discord Rich Presence support for MiSTer"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)
set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra -pedantic -Werror)

add_executable(mister-status)
add_subdirectory(src)

set(C_FILES "$<TARGET_PROPERTY:mister-status,SOURCES>")
set(H_FILES "$<TARGET_PROPERTY:mister-status,HEADER_SET>")

add_custom_target(
    tidy
    COMMAND clang-tidy --quiet "${C_FILES}" "${H_FILES}"
    COMMAND_EXPAND_LISTS
    VERBATIM
)

add_custom_target(
    format
    COMMAND clang-format -i --verbose "${C_FILES}" "${H_FILES}"
    COMMAND_EXPAND_LISTS
    VERBATIM
)

add_custom_target(
    format-check
    COMMAND clang-format --Werror --dry-run "${C_FILES}" "${H_FILES}"
    COMMAND_EXPAND_LISTS
    VERBATIM
)
