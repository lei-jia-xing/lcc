add_library(lexer lexer/Lexer.cpp)
add_library(parser parser/Parser.cpp)
add_library(semanticanalyzer semantic/SemanticAnalyzer.cpp)
add_library(errorReporter errorReporter/ErrorReporter.cpp)
add_library(codegen
    codegen/CodeGen.cpp
    codegen/QuadOptimizer.cpp
    codegen/Function.cpp
    codegen/BasicBlock.cpp
    codegen/Instruction.cpp
    codegen/Operand.cpp)

target_include_directories(lexer PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_include_directories(parser PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_include_directories(semanticanalyzer PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_include_directories(errorReporter PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_include_directories(codegen PUBLIC ${CMAKE_SOURCE_DIR}/include)
