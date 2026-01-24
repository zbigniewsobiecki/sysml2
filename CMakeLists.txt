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

# Core library (shared between main executable and tests)
add_library(sysml2_core STATIC
    src/arena.c
    src/intern.c
    src/keywords.c
    src/lexer.c
    src/diagnostic.c
    src/ast.c
    src/ast_builder.c
    src/json_writer.c
    src/validator.c
)

# Link math library on Unix
if(UNIX)
    target_link_libraries(sysml2_core m)
endif()

# Main executable
add_executable(sysml2
    src/main.c
    src/sysml_parser.c
)
target_link_libraries(sysml2 sysml2_core)

# Install
install(TARGETS sysml2 DESTINATION bin)

# Testing
enable_testing()

# Test executables
add_executable(test_lexer tests/test_lexer.c)
target_link_libraries(test_lexer sysml2_core)

# AST and JSON unit tests
add_executable(test_ast tests/test_ast.c)
target_link_libraries(test_ast sysml2_core)

# Validator unit tests
add_executable(test_validator tests/test_validator.c)
target_link_libraries(test_validator sysml2_core)

# PackCC-generated parser test
add_executable(test_packcc_parser
    src/sysml_parser.c
    tests/test_packcc_parser.c
)
target_include_directories(test_packcc_parser PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_packcc_parser sysml2_core)

# Add tests
add_test(NAME lexer_tests COMMAND test_lexer)
add_test(NAME ast_tests COMMAND test_ast)
add_test(NAME validator_tests COMMAND test_validator)

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

# PackCC parser tests - invalid syntax files (should fail with errors)
file(GLOB PACKCC_ERROR_FIXTURES
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/errors/*.sysml
)
foreach(fixture ${PACKCC_ERROR_FIXTURES})
    get_filename_component(fixture_name ${fixture} NAME_WE)
    get_filename_component(fixture_dir ${fixture} DIRECTORY)
    # Read expected error location from .expected file
    set(expected_file "${fixture_dir}/${fixture_name}.expected")
    if(EXISTS "${expected_file}")
        file(READ "${expected_file}" expected_loc)
        string(STRIP "${expected_loc}" expected_loc)
        # Test that parser fails and reports error at expected location
        add_test(NAME packcc_error_${fixture_name}
            COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_error_case.sh
                $<TARGET_FILE:test_packcc_parser> ${fixture} ${expected_loc}
        )
    endif()
endforeach()

# Legacy error reporting test (runs all error cases together)
add_test(NAME packcc_error_reporting
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_error_reporting.sh $<TARGET_FILE:test_packcc_parser>
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# JSON output fixture tests
file(GLOB JSON_FIXTURES ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/json/*.sysml)
foreach(fixture ${JSON_FIXTURES})
    get_filename_component(fixture_name ${fixture} NAME_WE)
    add_test(NAME json_output_${fixture_name}
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_json_output.sh
            $<TARGET_FILE:sysml2> ${fixture}
    )
endforeach()

# Validation integration tests
file(GLOB VALIDATION_FIXTURES
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/validation/*.sysml)
foreach(fixture ${VALIDATION_FIXTURES})
    get_filename_component(fixture_name ${fixture} NAME_WE)
    get_filename_component(fixture_dir ${fixture} DIRECTORY)
    set(expected_file "${fixture_dir}/${fixture_name}.expected")
    if(EXISTS "${expected_file}")
        add_test(NAME validation_${fixture_name}
            COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_validation.sh
                $<TARGET_FILE:sysml2> ${fixture} ${expected_file}
        )
    endif()
endforeach()

# Custom target for running tests
add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_lexer test_ast test_validator test_packcc_parser sysml2
)

# Print configuration summary
message(STATUS "")
message(STATUS "SysML v2 Parser Configuration:")
message(STATUS "  Version:      ${PROJECT_VERSION}")
message(STATUS "  Build type:   ${CMAKE_BUILD_TYPE}")
message(STATUS "  C Compiler:   ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "")
