cmake_minimum_required(VERSION 3.20)

project(BuildTool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(bt_core
    src/cli.cpp
    src/makefile.cpp
    src/planner.cpp
)
target_include_directories(bt_core PUBLIC include)
target_compile_features(bt_core PUBLIC cxx_std_17)

add_executable(build-tool src/main.cpp)
target_link_libraries(build-tool PRIVATE bt_core)

enable_testing()

add_executable(makefile_parser_test tests/makefile_parser_test.cpp)
target_link_libraries(makefile_parser_test PRIVATE bt_core)
add_test(NAME makefile_parser_test COMMAND makefile_parser_test)

add_executable(planner_test tests/planner_test.cpp)
target_link_libraries(planner_test PRIVATE bt_core)
add_test(NAME planner_test COMMAND planner_test)

add_executable(build_tool_e2e_test tests/build_tool_e2e_test.cpp)
add_dependencies(build_tool_e2e_test build-tool)
target_compile_definitions(build_tool_e2e_test PRIVATE
    BT_BINARY="$<TARGET_FILE:build-tool>")
add_test(NAME build_tool_e2e_test COMMAND build_tool_e2e_test)
