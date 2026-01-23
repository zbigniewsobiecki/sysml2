cmake_minimum_required(VERSION 3.16)

project(sysml2
    VERSION 0.1.0
    DESCRIPTION "SysML v2 Parser and Validator"
    LANGUAGES C
)

# C Standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Build type defaults
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# Compiler warnings
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Werror=implicit-function-declaration
        -Werror=return-type
        -Wno-unused-parameter
    )
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0)
    else()
        add_compile_options(-O2)
    endif()
elseif(MSVC)
    add_compile_options(/W4)
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Source files
set(SYSML2_SOURCES
    src/arena.c
    src/intern.c
    src/keywords.c
    src/lexer.c
    src/parser.c
    src/parser_expr.c
    src/ast.c
    src/symbol.c
    src/semantic.c
    src/diagnostic.c
    src/main.c
)

# Main executable
add_executable(sysml2 ${SYSML2_SOURCES})

# Link math library on Unix
if(UNIX)
    target_link_libraries(sysml2 m)
endif()

# Install
install(TARGETS sysml2 DESTINATION bin)

# Testing
enable_testing()

# Test executable
add_executable(test_lexer
    src/arena.c
    src/intern.c
    src/keywords.c
    src/lexer.c
    src/diagnostic.c
    tests/test_lexer.c
)
if(UNIX)
    target_link_libraries(test_lexer m)
endif()

add_executable(test_parser
    src/arena.c
    src/intern.c
    src/keywords.c
    src/lexer.c
    src/parser.c
    src/parser_expr.c
    src/ast.c
    src/diagnostic.c
    tests/test_parser.c
)
if(UNIX)
    target_link_libraries(test_parser m)
endif()

# Add tests
add_test(NAME lexer_tests COMMAND test_lexer)
add_test(NAME parser_tests COMMAND test_parser)

# Custom target for running tests
add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_lexer test_parser
)

# Print configuration summary
message(STATUS "")
message(STATUS "SysML v2 Parser Configuration:")
message(STATUS "  Version:      ${PROJECT_VERSION}")
message(STATUS "  Build type:   ${CMAKE_BUILD_TYPE}")
message(STATUS "  C Compiler:   ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "")
