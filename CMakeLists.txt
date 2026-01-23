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
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)  # for packcc-generated parser

# Source files
set(SYSML2_SOURCES
    src/arena.c
    src/intern.c
    src/keywords.c
    src/lexer.c
    src/sysml_parser.c   # packcc-generated parser
    src/ast.c
    src/symbol.c
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

# PackCC-generated parser test
add_executable(test_packcc_parser
    src/sysml_parser.c
    tests/test_packcc_parser.c
)
target_include_directories(test_packcc_parser PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Add tests
add_test(NAME lexer_tests COMMAND test_lexer)

# PackCC parser tests - valid syntax files
file(GLOB PACKCC_VALID_FIXTURES
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/official/*.sysml
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/official/phase2-basic/*.sysml
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/official/phase3-domain/*.sysml
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/official/phase3-domain/*/*.sysml
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/official/phase4-advanced/*/*.sysml
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/official/phase5-framework/*/*.sysml
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/official/phase5-framework/*/*/*.sysml
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/vehicle.sysml
)
foreach(fixture ${PACKCC_VALID_FIXTURES})
    get_filename_component(fixture_name ${fixture} NAME_WE)
    add_test(NAME packcc_valid_${fixture_name}
        COMMAND test_packcc_parser ${fixture}
    )
endforeach()

# PackCC parser tests - error reporting
add_test(NAME packcc_error_reporting
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_error_reporting.sh $<TARGET_FILE:test_packcc_parser>
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# Custom target for running tests
add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_lexer test_packcc_parser
)

# Print configuration summary
message(STATUS "")
message(STATUS "SysML v2 Parser Configuration:")
message(STATUS "  Version:      ${PROJECT_VERSION}")
message(STATUS "  Build type:   ${CMAKE_BUILD_TYPE}")
message(STATUS "  C Compiler:   ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "")
