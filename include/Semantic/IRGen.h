#ifndef _IRGEN_H
#define _IRGEN_H

#include <Semantic/AST.h>
#include <IR/IR.h>

#include <map>
#include <cstring>

namespace klang {

struct ModuleGenCtx {
  ModuleGenCtx(ASTModule* M) : M(M) {}

  const char* GetStringLiteral(const std::string& S) {
    auto It = StringLiterals.find(S);
    if(It == StringLiterals.end()) {
      auto Name = "__str" + std::to_string(StringLiterals.size());
      StringLiterals[S] = Name;
    }
    return StringLiterals[S].c_str();
  }

  ASTModule* M;
  std::map<std::string, std::string> StringLiterals;
};

struct FuncGenCtx {
  FuncGenCtx(ASTFunction* F, FuncBuilder* B, ModuleGenCtx* MCtx) : F(F), B(B), MCtx(MCtx) {}

  void InitVariables() {
    for(auto& V : F->GetVariables()) {
      Variables[V.first] = B->NewReg();
    }
  }

  Operand GetVariable(const ASTName& Name) {
    if(Variables.count(Name) > 0) {
      return Variables[Name];
    }

    size_t I = 0;
    for(auto Param : F->GetParameters()) {
      if(Param.first == Name) {
        return Operand::CreateParameter(I);
      }
      I++;
    }
    assert(false && "Variable not found");
    return Operand();
  }

  ASTFunction* F;
  FuncBuilder* B;
  ModuleGenCtx* MCtx;
  std::map<ASTName, Operand> Variables;
};

class IRGen {
public:
  IRGen(ASTModule* Module) : Module_(Module) {}

  bool Verify();
  std::pair<ModuleGenCtx, Module*> Generate();

private:
  Function* GenerateFunction(ModuleGenCtx& MCtx, ASTFunction* F);
  BasicBlock* GenerateBlock(FuncGenCtx& Ctx, const std::vector<ASTStatement*>& Statements);
  void GenerateStatement(FuncGenCtx& Ctx, ASTStatement* S);
  Operand GenerateExpression(FuncGenCtx& Ctx, ASTExpression* E, bool CallVoid = false);

  ASTModule* Module_;
};

} // namespace klang

#endif 