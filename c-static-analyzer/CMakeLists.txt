cmake_minimum_required(VERSION 3.20)

project(CStaticAnalyzer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# The C parser this tool analyzes is not compiled by this build — it's
# parsed at runtime by tree-sitter's C grammar, fetched here as source and
# built as a plain static library alongside the tree-sitter runtime itself.
FetchContent_Declare(
    tree_sitter
    GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter.git
    GIT_TAG v0.26.10
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(tree_sitter)

# tree-sitter-c ships its own CMakeLists.txt, but it wires up a custom
# command that regenerates src/parser.c via the tree-sitter CLI whenever
# CMake's dependency check thinks grammar.json is newer (which can happen
# spuriously right after a fresh checkout). parser.c is already checked
# into the grammar repo pre-generated, so we sidestep that entirely: fetch
# the source only (SOURCE_SUBDIR points at a directory that doesn't exist,
# so FetchContent never evaluates their CMakeLists.txt) and compile the
# checked-in parser.c ourselves.
FetchContent_Declare(
    tree_sitter_c
    GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter-c.git
    GIT_TAG v0.24.2
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR "cmake-disabled-subdir-does-not-exist"
)
FetchContent_MakeAvailable(tree_sitter_c)

add_library(tree_sitter_c_grammar STATIC
    ${tree_sitter_c_SOURCE_DIR}/src/parser.c
)
target_include_directories(tree_sitter_c_grammar PRIVATE ${tree_sitter_c_SOURCE_DIR}/src)
target_include_directories(tree_sitter_c_grammar INTERFACE ${tree_sitter_c_SOURCE_DIR}/bindings/c)
set_target_properties(tree_sitter_c_grammar PROPERTIES C_STANDARD 11)

add_library(sa_core
    src/diagnostic.cpp
    src/fnmatch.cpp
    src/config.cpp
    src/visitor.cpp
    src/analyzer.cpp
    src/cli.cpp
    src/snippet.cpp
    src/rules/sa001_complexity.cpp
    src/rules/sa002_unused_variables.cpp
    src/rules/sa003_nesting.cpp
    src/rules/sa004_missing_return.cpp
    src/rules/sa005_unreachable_code.cpp
    src/rules/sa006_uninitialized_variable.cpp
)
target_include_directories(sa_core PUBLIC include)
target_compile_features(sa_core PUBLIC cxx_std_20)
target_link_libraries(sa_core PUBLIC tree-sitter tree_sitter_c_grammar)

add_executable(c-static-analyzer src/main.cpp)
target_link_libraries(c-static-analyzer PRIVATE sa_core)

enable_testing()

add_executable(fnmatch_test tests/fnmatch_test.cpp)
target_link_libraries(fnmatch_test PRIVATE sa_core)
add_test(NAME fnmatch_test COMMAND fnmatch_test)

add_executable(config_test tests/config_test.cpp)
target_link_libraries(config_test PRIVATE sa_core)
add_test(NAME config_test COMMAND config_test)

add_executable(analyzer_test tests/analyzer_test.cpp)
target_link_libraries(analyzer_test PRIVATE sa_core)
add_test(NAME analyzer_test COMMAND analyzer_test)

add_executable(sa001_complexity_test tests/sa001_complexity_test.cpp)
target_link_libraries(sa001_complexity_test PRIVATE sa_core)
add_test(NAME sa001_complexity_test COMMAND sa001_complexity_test)

add_executable(sa002_unused_variables_test tests/sa002_unused_variables_test.cpp)
target_link_libraries(sa002_unused_variables_test PRIVATE sa_core)
add_test(NAME sa002_unused_variables_test COMMAND sa002_unused_variables_test)

add_executable(sa003_nesting_test tests/sa003_nesting_test.cpp)
target_link_libraries(sa003_nesting_test PRIVATE sa_core)
add_test(NAME sa003_nesting_test COMMAND sa003_nesting_test)

add_executable(sa004_missing_return_test tests/sa004_missing_return_test.cpp)
target_link_libraries(sa004_missing_return_test PRIVATE sa_core)
add_test(NAME sa004_missing_return_test COMMAND sa004_missing_return_test)

add_executable(sa005_unreachable_code_test tests/sa005_unreachable_code_test.cpp)
target_link_libraries(sa005_unreachable_code_test PRIVATE sa_core)
add_test(NAME sa005_unreachable_code_test COMMAND sa005_unreachable_code_test)

add_executable(sa006_uninitialized_variable_test tests/sa006_uninitialized_variable_test.cpp)
target_link_libraries(sa006_uninitialized_variable_test PRIVATE sa_core)
add_test(NAME sa006_uninitialized_variable_test COMMAND sa006_uninitialized_variable_test)

add_executable(snippet_test tests/snippet_test.cpp)
target_link_libraries(snippet_test PRIVATE sa_core)
add_test(NAME snippet_test COMMAND snippet_test)

add_executable(golden_test tests/golden_test.cpp)
add_dependencies(golden_test c-static-analyzer)
target_compile_definitions(golden_test PRIVATE
    SA_BINARY="$<TARGET_FILE:c-static-analyzer>"
    SA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_test(NAME golden_test COMMAND golden_test)

add_executable(cli_test tests/cli_test.cpp)
add_dependencies(cli_test c-static-analyzer)
target_compile_definitions(cli_test PRIVATE
    SA_BINARY="$<TARGET_FILE:c-static-analyzer>")
add_test(NAME cli_test COMMAND cli_test)
