cmake_minimum_required(VERSION 3.20)

project(CLinker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(clnk_core
    src/elf_reader.cpp
    src/elf_writer.cpp
    src/diagnostic.cpp
    src/symbol_resolver.cpp
    src/section_merger.cpp
    src/relocation_applier.cpp
    src/linker.cpp
    src/cli.cpp
)
target_include_directories(clnk_core PUBLIC include)
target_compile_features(clnk_core PUBLIC cxx_std_20)

add_executable(c-link src/main.cpp)
target_link_libraries(c-link PRIVATE clnk_core)

# c-link itself has zero external dependencies at runtime. clang is only
# needed to compile this subproject's own test/example fixtures (real,
# small, freestanding C sources) into the real ELF object files the tests
# feed to the linker -- see tests/support/compile_fixture.h.
find_program(CLANG_EXECUTABLE NAMES clang)
if(NOT CLANG_EXECUTABLE)
    message(FATAL_ERROR "clang not found: required to compile c-linker's test/example fixtures into ELF object files")
endif()

enable_testing()

function(clnk_add_test name)
    add_executable(${name} tests/${name}.cpp)
    target_link_libraries(${name} PRIVATE clnk_core)
    target_compile_definitions(${name} PRIVATE
        CLNK_CLANG="${CLANG_EXECUTABLE}"
        CLNK_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures")
    add_test(NAME ${name} COMMAND ${name})
endfunction()

clnk_add_test(elf_reader_test)
clnk_add_test(symbol_resolver_test)
clnk_add_test(section_merger_test)
clnk_add_test(relocation_test)
clnk_add_test(linker_test)

add_executable(cli_test tests/cli_test.cpp)
target_link_libraries(cli_test PRIVATE clnk_core)
add_dependencies(cli_test c-link)
target_compile_definitions(cli_test PRIVATE
    CLNK_CLANG="${CLANG_EXECUTABLE}"
    CLNK_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures"
    CLNK_BINARY="$<TARGET_FILE:c-link>")
add_test(NAME cli_test COMMAND cli_test)
