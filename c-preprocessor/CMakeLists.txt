cmake_minimum_required(VERSION 3.20)

project(CPreprocessor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(pp_core
    src/diagnostics.cpp
    src/comment_stripper.cpp
    src/pp_tokenizer.cpp
    src/macro_table.cpp
    src/macro_expander.cpp
    src/preprocessor.cpp
)
target_include_directories(pp_core PUBLIC include)
target_compile_features(pp_core PUBLIC cxx_std_20)

add_executable(c-preprocess src/main.cpp)
target_link_libraries(c-preprocess PRIVATE pp_core)

enable_testing()

add_executable(comment_stripper_test tests/comment_stripper_test.cpp)
target_link_libraries(comment_stripper_test PRIVATE pp_core)
add_test(NAME comment_stripper_test COMMAND comment_stripper_test)

add_executable(pp_tokenizer_test tests/pp_tokenizer_test.cpp)
target_link_libraries(pp_tokenizer_test PRIVATE pp_core)
add_test(NAME pp_tokenizer_test COMMAND pp_tokenizer_test)

add_executable(macro_expander_test tests/macro_expander_test.cpp)
target_link_libraries(macro_expander_test PRIVATE pp_core)
add_test(NAME macro_expander_test COMMAND macro_expander_test)

add_executable(directive_test tests/directive_test.cpp)
target_link_libraries(directive_test PRIVATE pp_core)
add_test(NAME directive_test COMMAND directive_test)

add_executable(include_resolution_test tests/include_resolution_test.cpp)
target_link_libraries(include_resolution_test PRIVATE pp_core)
target_compile_definitions(include_resolution_test PRIVATE
    PP_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures")
add_test(NAME include_resolution_test COMMAND include_resolution_test)

add_executable(preprocessor_test tests/preprocessor_test.cpp)
target_link_libraries(preprocessor_test PRIVATE pp_core)
target_compile_definitions(preprocessor_test PRIVATE
    PP_EXAMPLES_DIR="${CMAKE_SOURCE_DIR}/examples")
add_test(NAME preprocessor_test COMMAND preprocessor_test)

add_executable(cli_test tests/cli_test.cpp)
add_dependencies(cli_test c-preprocess)
target_compile_definitions(cli_test PRIVATE
    PP_BINARY="$<TARGET_FILE:c-preprocess>"
    PP_EXAMPLES_DIR="${CMAKE_SOURCE_DIR}/examples"
    PP_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures")
add_test(NAME cli_test COMMAND cli_test)
