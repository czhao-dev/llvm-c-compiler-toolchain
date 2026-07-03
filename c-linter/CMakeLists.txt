cmake_minimum_required(VERSION 3.20)

project(CLinter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(cl_core
    src/token.cpp
    src/lexer.cpp
    src/diagnostic.cpp
    src/line_rules.cpp
    src/naming_rule.cpp
    src/magic_number_rule.cpp
    src/brace_style_rule.cpp
    src/linter.cpp
)
target_include_directories(cl_core PUBLIC include)
target_compile_features(cl_core PUBLIC cxx_std_20)

add_executable(c-lint src/main.cpp)
target_link_libraries(c-lint PRIVATE cl_core)

enable_testing()

add_executable(lexer_test tests/lexer_test.cpp)
target_link_libraries(lexer_test PRIVATE cl_core)
add_test(NAME lexer_test COMMAND lexer_test)

add_executable(naming_rule_test tests/naming_rule_test.cpp)
target_link_libraries(naming_rule_test PRIVATE cl_core)
add_test(NAME naming_rule_test COMMAND naming_rule_test)

add_executable(line_rules_test tests/line_rules_test.cpp)
target_link_libraries(line_rules_test PRIVATE cl_core)
add_test(NAME line_rules_test COMMAND line_rules_test)

add_executable(magic_number_rule_test tests/magic_number_rule_test.cpp)
target_link_libraries(magic_number_rule_test PRIVATE cl_core)
add_test(NAME magic_number_rule_test COMMAND magic_number_rule_test)

add_executable(brace_style_rule_test tests/brace_style_rule_test.cpp)
target_link_libraries(brace_style_rule_test PRIVATE cl_core)
add_test(NAME brace_style_rule_test COMMAND brace_style_rule_test)

add_executable(linter_test tests/linter_test.cpp)
target_link_libraries(linter_test PRIVATE cl_core)
target_compile_definitions(linter_test PRIVATE
    CL_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures"
    CL_EXAMPLES_DIR="${CMAKE_SOURCE_DIR}/examples")
add_test(NAME linter_test COMMAND linter_test)

add_executable(cli_test tests/cli_test.cpp)
add_dependencies(cli_test c-lint)
target_compile_definitions(cli_test PRIVATE
    CL_BINARY="$<TARGET_FILE:c-lint>"
    CL_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures")
add_test(NAME cli_test COMMAND cli_test)
