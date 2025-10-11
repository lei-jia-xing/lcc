add_library(lexer lexer/Lexer.cpp)
add_library(parser parser/Parser.cpp)

target_include_directories(lexer PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_include_directories(parser PUBLIC ${CMAKE_SOURCE_DIR}/include)
