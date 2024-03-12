FLEX_Target(klangLexer Semantic/Scanner.ll ${CMAKE_CURRENT_BINARY_DIR}/Scanner.cpp)
BISON_Target(klangParser Semantic/Parser.yy ${CMAKE_CURRENT_BINARY_DIR}/Parser.cpp DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/Parser.h)
add_flex_bison_dependency(klangLexer klangParser)

add_executable(klang 
  Main.cpp
  Logging.cpp
  Util.cpp

  IR/IR.cpp
  IR/Interpreter.cpp
  IR/Analysis.cpp
  IR/Optimize.cpp
  
  Codegen/Codegen.cpp
  Codegen/RegAlloc.cpp
  Codegen/InstSched.cpp

  Semantic/AST.cpp
  Semantic/IRGen.cpp 

  ${FLEX_klangLexer_OUTPUTS}
  ${BISON_klangParser_OUTPUTS}
)
target_include_directories(klang PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

if(ASAN_ENABLED)
  target_compile_options(klang PRIVATE -fsanitize=address)
  target_link_options(klang PRIVATE -fsanitize=address)
endif()