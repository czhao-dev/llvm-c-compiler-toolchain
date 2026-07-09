cmake_minimum_required(VERSION 3.20)

project(MiniCCompiler LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MINIC_ENABLE_LLVM "Try to discover and link LLVM for future codegen work" ON)

add_library(minic_core
    src/lexer.cpp
    src/ast.cpp
    src/parser.cpp
    src/sema.cpp
    src/codegen.cpp
)
target_include_directories(minic_core PUBLIC include)
target_compile_features(minic_core PUBLIC cxx_std_20)

if(MINIC_ENABLE_LLVM)
    find_package(LLVM CONFIG QUIET)
    if(LLVM_FOUND)
        message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}: ${LLVM_DIR}")
        target_compile_definitions(minic_core PUBLIC MINIC_HAS_LLVM=1)
        target_include_directories(minic_core SYSTEM PUBLIC ${LLVM_INCLUDE_DIRS})
        separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND "${LLVM_DEFINITIONS}")
        target_compile_definitions(minic_core PUBLIC ${LLVM_DEFINITIONS_LIST})
        llvm_map_components_to_libnames(LLVM_LIBS core support irreader passes)
        target_link_libraries(minic_core PUBLIC ${LLVM_LIBS})
    else()
        message(STATUS "LLVM not found. Lexer/parser setup will still build.")
    endif()
endif()

add_executable(minic src/main.cpp)
target_link_libraries(minic PRIVATE minic_core)

enable_testing()

add_executable(lexer_test tests/lexer_test.cpp)
target_link_libraries(lexer_test PRIVATE minic_core)
add_test(NAME lexer_test COMMAND lexer_test)

add_executable(smoke_test tests/smoke_test.cpp)
target_link_libraries(smoke_test PRIVATE minic_core)
add_test(NAME smoke_test COMMAND smoke_test)

add_executable(parser_test tests/parser_test.cpp)
target_link_libraries(parser_test PRIVATE minic_core)
target_compile_definitions(parser_test PRIVATE MINIC_EXAMPLES_DIR="${CMAKE_SOURCE_DIR}/examples")
add_test(NAME parser_test COMMAND parser_test)

add_executable(sema_test tests/sema_test.cpp)
target_link_libraries(sema_test PRIVATE minic_core)
target_compile_definitions(sema_test PRIVATE MINIC_EXAMPLES_DIR="${CMAKE_SOURCE_DIR}/examples")
add_test(NAME sema_test COMMAND sema_test)

if(MINIC_ENABLE_LLVM AND LLVM_FOUND)
    add_executable(codegen_test tests/codegen_test.cpp)
    target_link_libraries(codegen_test PRIVATE minic_core)
    target_compile_definitions(codegen_test PRIVATE MINIC_EXAMPLES_DIR="${CMAKE_SOURCE_DIR}/examples")
    add_test(NAME codegen_test COMMAND codegen_test)

    find_program(CLANG_EXECUTABLE clang)
    if(CLANG_EXECUTABLE)
        set(_example_outputs)
        foreach(prog fibonacci fizzbuzz gcd sum_of_squares pointer_swap array_sum struct_point bit_ops control_flow)
            add_custom_command(
                OUTPUT ${CMAKE_BINARY_DIR}/${prog}_clang
                COMMAND ${CLANG_EXECUTABLE} -x c -include stdio.h
                        ${CMAKE_SOURCE_DIR}/examples/${prog}.mc
                        -o ${CMAKE_BINARY_DIR}/${prog}_clang
                DEPENDS ${CMAKE_SOURCE_DIR}/examples/${prog}.mc
                VERBATIM
            )
            add_custom_command(
                OUTPUT ${CMAKE_BINARY_DIR}/${prog}_minic
                COMMAND $<TARGET_FILE:minic>
                        ${CMAKE_SOURCE_DIR}/examples/${prog}.mc
                        -o ${CMAKE_BINARY_DIR}/${prog}_minic
                DEPENDS minic ${CMAKE_SOURCE_DIR}/examples/${prog}.mc
                VERBATIM
            )
            list(APPEND _example_outputs
                ${CMAKE_BINARY_DIR}/${prog}_clang
                ${CMAKE_BINARY_DIR}/${prog}_minic
            )
        endforeach()
        add_custom_target(examples ALL DEPENDS ${_example_outputs})
    endif()
endif()
