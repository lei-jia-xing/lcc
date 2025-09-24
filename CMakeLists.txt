cmake_minimum_required(VERSION 3.28)

project(Compiler)

set(CMAKE_CXX_STANDARD 17)

# 添加 include 目录，以便编译器找到 "lexer/Lexer.hpp"
include_directories(${PROJECT_SOURCE_DIR}/include)

# 处理 src 子目录，这会定义 lexer 库
add_subdirectory(src)

# 定义最终的可执行文件 lcc
add_executable(lcc main.cpp)

# 将 lexer 库链接到 lcc 可执行文件
target_link_libraries(lcc lexer)