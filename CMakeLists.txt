cmake_minimum_required(VERSION 3.10)

project(Compiler)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include_directories(${PROJECT_SOURCE_DIR}/include)

add_subdirectory(src)

add_executable(Compiler main.cpp)

target_link_libraries(Compiler Lexer Parser Semanticanalyzer
ErrorReporter Codegen Backend)

# Linter and formatter targets
file(GLOB_RECURSE ALL_SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/src/*.cpp
    ${CMAKE_SOURCE_DIR}/src/*.h
    ${CMAKE_SOURCE_DIR}/include/*.h
    ${CMAKE_SOURCE_DIR}/*.cpp)

add_custom_target(format
    COMMAND clang-format -i ${ALL_SOURCE_FILES}
    COMMENT "Formatting code with clang-format")

add_custom_target(lint
    COMMAND clang-tidy ${ALL_SOURCE_FILES} -- -I${CMAKE_SOURCE_DIR}/include
    COMMENT "Linting code with clang-tidy")
