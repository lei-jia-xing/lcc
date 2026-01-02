add_library(Lexer lexer/Lexer.cpp)
add_library(Parser parser/Parser.cpp)
add_library(Semanticanalyzer semantic/SemanticAnalyzer.cpp)
add_library(ErrorReporter errorReporter/ErrorReporter.cpp)
add_library(Codegen
    codegen/CodeGen.cpp
    codegen/QuadOptimizer.cpp
    codegen/Function.cpp
    codegen/BasicBlock.cpp
    codegen/Instruction.cpp
    codegen/Operand.cpp
    optimize/DominatorTree.cpp
    optimize/LoopAnalysis.cpp
    optimize/LICM.cpp
    optimize/Mem2Reg.cpp
    optimize/PhiElimination.cpp
    optimize/GlobalConstEval.cpp
    optimize/LoopUnroll.cpp
    )

add_library(Backend
    backend/AsmGen.cpp
    backend/RegisterAllocator.cpp)

target_include_directories(Backend PUBLIC ${CMAKE_SOURCE_DIR}/include)

target_include_directories(Lexer PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_include_directories(Parser PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_include_directories(Semanticanalyzer PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_include_directories(ErrorReporter PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_include_directories(Codegen PUBLIC ${CMAKE_SOURCE_DIR}/include)
