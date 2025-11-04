add_library(lexer lexer/Lexer.cpp)
add_library(parser parser/Parser.cpp)
add_library(semanticanalyzer semantic/SemanticAnalyzer.cpp)

target_include_directories(lexer PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_include_directories(parser PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_include_directories(semanticanalyzer PUBLIC ${CMAKE_SOURCE_DIR}/include)
