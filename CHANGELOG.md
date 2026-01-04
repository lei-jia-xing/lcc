## # LCC 编译器 - CHANGELOG [unreleased]

### Features

- Add DomTree and LICM analysis
- Add some some optimize
- Add PHI support for SSA
- Add CSE pass
- Add Phi support
- Add Mem2Reg pass
- Add coloring register
- Add NOP support
- Add debug function dumpCFG
- Make Phi result mutable to fit the optimize need
- Mem2Reg pass
- Add GlobalConstEval pass
- Add strength reductioin
- Add CFG cleanup pass
- Add inliner pass
- Add loopUnroll pass
- Add store & load support for globalConstEval
- Add SCCP pass
- Final commit

### Bug Fixes

- Prevent wrong LICM for variabel
- Tempid should be managed by function
- Doc table fix
- Correctly handle the dominator by checking if/goto/return
- Const array element should be evaluated in compile time
- LICM pass
- Several pass bug
- LICM TLE bug
- Remove Inliner pass
- Remove CFGCleanup

### Other

- Cliff to show changelog
- Add independent phielimination pass
- Make param ir atomic for optimizing purpose

### Documentation

- Fix serval bug
- Add optimize article
- Complete docs

### Performance

- Optimize pass order & structure

### Miscellaneous Tasks

- Clean script
- Merge goto & if into one block
- Add phiElimination into cmake
- Add linter and formatter configuration
- Add some comment & clean code
- Instruction initializer
- Add comment & add doxyfile config
- Update doxyfile

## [1.0.0] - 2025-12-06

### Features

- Lexer complete
- Add AST classes
- Add Parser class
- Add error func in lexer
- Add implements of Parser
- Add cout
- Add support for silentPV
- Pass the example
- Add output in lexer
- Add error recovery
- Make parser more robust
- Parser complete
- Add doc into zip for exam
- Define the data strcuture relavant to semantic
- The basic data struct of semantic process
- The initial implement of semanticAnalyzer
- Add outputEnable to control output
- Add closing BraceLine info
- Change the errorStream and outputSteam
- Add ErrorReporter to collect error info
- Use errorReporter to print
- Use errorReporter
- Example pass
- Complete semantic
- Overall codegen frame
- Basic definition of codegen
- Full use of function structure
- Add static and constExp evaluate
- Add backend
- Add funtion support in codegen
- Add the mips.txt output
- Add asmgen
- Add type property for VarDef ASTNode
- Add the scratch register num
- Make difference between array and int
- The union coding
- Add printf argslist support
- Add quit test in script
- Remove namespace & finish regalloc
- Improve reg manager & fix serveral bug
- Ready for big refactor
- Add getSymbolTable
- A large refactor
- Add defence code
- Add support for static
- Change from label->name to name->label
- Add constprop pass & algebra pass
- Add CopyProp & constProp pass
- Add sll for 2^n mul

### Bug Fixes

- Fix RE bug for Lexer
- Correctly order skipwhitespace and finish signal
- Correctly map OpType with TokenType
- Change the error define
- Change the fileio to adjust the judge
- Amend advance in neccesary
- Fix logic bug
- Fix the NEQ output
- Change latex engine to pdflatex to best suit for doxygen
- Make the Lexer more rubar
- The error output should be limited by silentDepth
- Get rid of useless include
- Change the AST string type into TypePtr
- Error type g
- Several bug
- Fixup the class definition
- Amend the unaryExp
- Fit for no namespace
- Static global name & array param
- Add support for runtime lower global initial & store array param
- Add unique Name for func
- Make code clear
- The abuse of scratch reg (#4)

### Other

- Compiler init!!!
- Add Parser into cmake
- Add parser link
- Fix the ignore
- Add semanticanalyzer to cmake target
- Add errorReporter into target
- Add complie_commands
- Add OpType for LAnd & LOr

### Refactor

- Make mips suit for MIPS32 standard

### Documentation

- Add doxygen config
- Add lexer docs
- Add lexer docs in doxygen style
- Add Token docs in doxygen style
- Change docs to consistent style
- Add the parser docs in doxygen style
- Add parser docs and amend parser docs
- Add the whole compiler intro
- Update docs
- Doxygenfile update
- Update completation
- Update EBNF possible semantic error
- Update docs in semantic process
- Update docs
- Sync docs & code
- Update documention
- Update docs
- Update docs

### Miscellaneous Tasks

- Suit for buaa test
- Add a LICENESE
- Add some docs
- Update gitignore
- Add actual copyright
- Update EBNF
- Add error illustration
- Fit lexer init with parser init
- Ignore the pdf file
- Changge the EOF TypeNmae
- Sort code
- Move markdown into doc
- Sort code
- Change the name of lexer-design doc
- Full use of smart pointer
- Shutdown the output of parser and lexer
- Get rid of something useless
- Clean the code
- Update docs and clean code
- Clean code
- Add doc
- Clean copilot useless code
- Clean code
- Clean code & add code source
- Clean printf
- Add guard code
- Align
- Update docs and do some clean
