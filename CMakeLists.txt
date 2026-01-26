cmake_minimum_required(VERSION 3.16)

project(sysml2
    VERSION 0.1.0
    DESCRIPTION "SysML v2 CLI"
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

# Parser generation from grammar using packcc
find_program(PACKCC packcc)
if(PACKCC)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/src/sysml_parser.c
        COMMAND ${PACKCC} -o ${CMAKE_CURRENT_SOURCE_DIR}/src/sysml_parser
                ${CMAKE_CURRENT_SOURCE_DIR}/grammar/sysml.peg
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/grammar/sysml.peg
        COMMENT "Regenerating parser from grammar/sysml.peg"
    )
    message(STATUS "PackCC found: ${PACKCC} - parser will be regenerated when grammar changes")
else()
    message(STATUS "PackCC not found - using pre-generated parser")
endif()

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
    src/sysml_writer.c
    src/validator.c
    src/import_resolver.c
    src/utils.c
    src/pipeline.c
    src/symtab.c
    src/query.c
    src/modify.c
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

# Query unit tests
add_executable(test_query tests/test_query.c)
target_link_libraries(test_query sysml2_core)

# Modify unit tests
add_executable(test_modify
    src/sysml_parser.c
    tests/test_modify.c
)
target_include_directories(test_modify PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_modify sysml2_core)

# PackCC-generated parser test
add_executable(test_packcc_parser
    src/sysml_parser.c
    tests/test_packcc_parser.c
)
target_include_directories(test_packcc_parser PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_packcc_parser sysml2_core)

# Import resolver unit tests
add_executable(test_import_resolver
    src/sysml_parser.c
    tests/test_import_resolver.c
)
target_include_directories(test_import_resolver PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_import_resolver sysml2_core)

# Diagnostic unit tests
add_executable(test_diagnostic tests/test_diagnostic.c)
target_link_libraries(test_diagnostic sysml2_core)

# JSON writer unit tests
add_executable(test_json_writer tests/test_json_writer.c)
target_link_libraries(test_json_writer sysml2_core)

# SysML writer unit tests
add_executable(test_sysml_writer
    src/sysml_parser.c
    tests/test_sysml_writer.c
)
target_include_directories(test_sysml_writer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_sysml_writer sysml2_core)

# Memory (arena/intern) unit tests
add_executable(test_memory tests/test_memory.c)
target_link_libraries(test_memory sysml2_core)

# Add tests
add_test(NAME lexer_tests COMMAND test_lexer)
add_test(NAME ast_tests COMMAND test_ast)
add_test(NAME validator_tests COMMAND test_validator)
add_test(NAME query_tests COMMAND test_query)
add_test(NAME modify_tests COMMAND test_modify)
add_test(NAME import_resolver_tests COMMAND test_import_resolver)
add_test(NAME diagnostic_tests COMMAND test_diagnostic)
add_test(NAME json_writer_tests COMMAND test_json_writer)
add_test(NAME sysml_writer_tests COMMAND test_sysml_writer)
add_test(NAME memory_tests COMMAND test_memory)

# PackCC parser tests - valid syntax files
file(GLOB PACKCC_VALID_FIXTURES
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/official/*.sysml
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/official/phase2-basic/*.sysml
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/official/phase3-domain/*.sysml
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/official/phase3-domain/*/*.sysml
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/official/phase4-advanced/*/*.sysml
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/official/phase5-framework/*/*.sysml
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/official/phase5-framework/*/*/*.sysml
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/valid/*.sysml
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/modify/*.sysml
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
file(GLOB JSON_FIXTURES
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/json/*.sysml
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/json/*.kerml)
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

# Pretty printing (SysML output) fixture tests
file(GLOB FORMAT_FIXTURES
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/format/*.input.sysml)
foreach(fixture ${FORMAT_FIXTURES})
    get_filename_component(fixture_name ${fixture} NAME_WE)
    # Remove .input suffix for cleaner test name
    string(REPLACE ".input" "" clean_name ${fixture_name})
    add_test(NAME format_${clean_name}
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_sysml_output.sh
            $<TARGET_FILE:sysml2> ${fixture}
    )
endforeach()

# Cross-file --fix mode test
add_test(NAME format_cross_file
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_sysml_fix.sh
        $<TARGET_FILE:sysml2>
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/format/10-cross-file
)

# CLI CRUD operations integration test
add_test(NAME cli_crud_integration
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cli_crud.sh
        $<TARGET_FILE:sysml2>
)

# Exit code tests - verifies 0/1/2 exit codes for success/parse/semantic errors
add_test(NAME cli_exit_codes
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_exit_codes.sh
        $<TARGET_FILE:sysml2>
)

# Package discovery tests - verifies that imports work when filename != package name
add_test(NAME cli_package_discovery
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_package_discovery.sh
        $<TARGET_FILE:sysml2>
)

# Ensure tests run without external library path pollution
get_property(all_tests DIRECTORY PROPERTY TESTS)
set_tests_properties(${all_tests} PROPERTIES
    ENVIRONMENT "SYSML2_LIBRARY_PATH="
)

# Custom target for running tests
add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_lexer test_ast test_validator test_query test_modify test_packcc_parser
            test_import_resolver test_diagnostic test_json_writer test_sysml_writer test_memory
            sysml2
)

# Print configuration summary
message(STATUS "")
message(STATUS "SysML v2 Parser Configuration:")
message(STATUS "  Version:      ${PROJECT_VERSION}")
message(STATUS "  Build type:   ${CMAKE_BUILD_TYPE}")
message(STATUS "  C Compiler:   ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "")
