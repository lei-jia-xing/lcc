cmake_minimum_required(VERSION 3.10)

project(Compiler)

set(CMAKE_CXX_STANDARD 17)

include_directories(${PROJECT_SOURCE_DIR}/include)

add_subdirectory(src)

add_executable(Compiler main.cpp)

target_link_libraries(Compiler lexer parser semanticanalyzer errorReporter)
