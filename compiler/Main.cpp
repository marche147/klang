#include <IR/IR.h>
#include <IR/Optimize.h>

#include <Codegen/Codegen.h>

#include <Semantic/Scanner.h>
#include "Parser.h"
#include <Semantic/IRGen.h>

#include <iostream>
#include <fstream>

using namespace klang;

#define DEBUG 1

namespace klang {

ASTModule* ParseSource(const char* FileName) {
  std::ifstream Input(FileName, std::ifstream::in);
  if(!Input.is_open()) {
    std::cerr << "Error: cannot open file " << FileName << std::endl;
    return nullptr;
  }

  Scanner S(&Input);
  Parser P(S);
  if(P.parse() != 0) {
    std::cerr << "Error: parsing failed" << std::endl;
    return nullptr;
  }
  return GetModule();
}

int Compile(const char* FileName, const char* OutputName = "out.S") {
  auto *Module = ParseSource(FileName);
  if(!Module) {
    return 1;
  }

  // external function defs
  Module->AddExternalFunction("printi", std::make_pair(TY_VOID, std::vector<ASTType>{TY_INTEGER}));
  Module->AddExternalFunction("prints", std::make_pair(TY_VOID, std::vector<ASTType>{TY_STRING}));
  Module->AddExternalFunction("inputi", std::make_pair(TY_INTEGER, std::vector<ASTType>{}));
  Module->AddExternalFunction("inputs", std::make_pair(TY_STRING, std::vector<ASTType>{}));
  Module->AddExternalFunction("random", std::make_pair(TY_INTEGER, std::vector<ASTType>{}));
  Module->AddExternalFunction("array_new", std::make_pair(TY_ARRAY, std::vector<ASTType>{TY_INTEGER}));

  IRGen Gen(Module);
  if(!Gen.Verify()) {
    return 1;
  }

  auto [MCtx, M] = Gen.Generate();
  for(auto *F : (*M)) {
    OptimizeIR(F);
  }

#if DEBUG
  for(auto *F : (*M)) {
    F->Print();
  }
#endif 

  ModuleCodegen Codegen(M, &MCtx);
  if(!Codegen.Generate()) {
    return 1;
  }
  if(!Codegen.Save(OutputName)) {
    return 1;
  }
  return 0;
}

} // namespace klang

int main(int argc, const char **argv) {
  if(argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <source file> [output file]" << std::endl;
    return 1;
  }

  if(argc == 2) {
    return klang::Compile(argv[1]);
  }
  return klang::Compile(argv[1], argv[2]);
}