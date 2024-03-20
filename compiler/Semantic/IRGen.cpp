#include <Semantic/IRGen.h>
#include <Logging.h>

namespace klang {

static std::optional<ASTType> VerifyExpression(ASTExpression* E);
static bool VerifyBlock(const std::vector<ASTStatement*>& Statements);
static bool VerifyStatement(ASTStatement* S);
static bool VerifyFunction(ASTFunction* F);

static std::optional<ASTType> FindVariable(const ASTName& Name, ASTFunction* F) {
  auto &V = F->GetVariables();
  auto &P = F->GetParameters();
  for(auto &Var : V) {
    if(Var.first == Name) {
      return Var.second;
    }
  }
  for(auto &Param : P) {
    if(Param.first == Name) {
      return Param.second;
    }
  }
  return std::nullopt;
}

static std::optional<ASTType> VerifyExpression(ASTExpression* E) {
  switch(E->GetType()) {
    case ASTExpression::EX_INTEGER: {
      return TY_INTEGER;
    }
    case ASTExpression::EX_STRING: {
      return TY_STRING;
    }
    case ASTExpression::EX_VARIABLE: {
      auto *VE = static_cast<ASTExpressionVariable*>(E);
      auto T = FindVariable(VE->GetName(), VE->GetParent()->GetParent());
      if(!T.has_value()) {
        ERROR("Undefined variable %s at line %d\n", VE->GetName().c_str(), E->GetParent()->GetLineNo());
      }
      return T;
    }
    case ASTExpression::EX_BINARY: {
      auto *BE = static_cast<ASTExpressionBinary*>(E);
      auto LT = VerifyExpression(BE->GetLHS());
      auto RT = VerifyExpression(BE->GetRHS());
      if(LT.has_value() && RT.has_value()) {
        if(LT.value() != RT.value()) {
          ERROR("Type mismatch in binary expression at line %d\n", E->GetParent()->GetLineNo());
          return std::nullopt;
        }
        if(LT.value() != TY_INTEGER) {
          ERROR("Invalid type in binary expression at line %d\n", E->GetParent()->GetLineNo());
          return std::nullopt;
        }
        return LT;
      }
    }
    case ASTExpression::EX_FUNCTION_CALL: {
      auto *FCE = static_cast<ASTExpressionFunctionCall*>(E);
      auto Proto = E->GetParent()->GetParent()->GetParent()->GetPrototype(FCE->GetName().c_str());
      if(!Proto.has_value()) {
        ERROR("Call to undefined function %s at line %d\n", FCE->GetName().c_str(), E->GetParent()->GetLineNo());
        return std::nullopt;
      }
      auto &Params = Proto.value().second;
      if(Params.size() != FCE->GetArguments().size()) {
        ERROR("Invalid number of arguments in function call at line %d\n", E->GetParent()->GetLineNo());
        return std::nullopt;
      }
      for(size_t i = 0; i < Params.size(); i++) {
        auto ET = VerifyExpression(FCE->GetArguments()[i]);
        if(!ET.has_value()) {
          return std::nullopt;
        }
        if(ET.value() != Params[i]) {
          ERROR("Type mismatch in function call (argument %d) at line %d\n", i, E->GetParent()->GetLineNo());
          return std::nullopt;
        }
      }
      return Proto.value().first;
    }
    case ASTExpression::EX_ARRAY_ACCESS: {
      auto *AE = static_cast<ASTExpressionArrayAccess*>(E);
      auto T = FindVariable(AE->GetName(), AE->GetParent()->GetParent());
      if(!T.has_value()) {
        ERROR("Undefined variable %s at line %d\n", AE->GetName().c_str(), E->GetParent()->GetLineNo());
        return std::nullopt;
      }
      if(T.value() != TY_ARRAY) {
        ERROR("Invalid type in array access expression at line %d\n", E->GetParent()->GetLineNo());
        return std::nullopt;
      }

      auto IT = VerifyExpression(AE->GetIndex());
      if(!IT.has_value()) {
        return std::nullopt;
      }
      if(IT.value() != TY_INTEGER) {
        ERROR("Invalid type in array index expression at line %d\n", E->GetParent()->GetLineNo());
        return std::nullopt;
      }
      return TY_INTEGER;
    }
  }
}

static bool VerifyBlock(const std::vector<ASTStatement*>& Statements) {
  for(auto *S : Statements) {
    if(!VerifyStatement(S)) {
      return false;
    }
  }

  // if the block contains return statement
  // it's only valid if it's the last statement
  for(auto *S : Statements) {
    if(S->GetType() == ASTStatement::ST_RETURN) {
      if(S != Statements.back()) {
        ERROR("Return statement is not the last statement in the block, at line %d\n", S->GetLineNo());
        return false;
      }
    }
  }
  return true;
}

static bool VerifyStatement(ASTStatement* S) {
  switch(S->GetType()) {
    case ASTStatement::ST_ASSIGN: {
      auto *AS = static_cast<ASTStatementAssign*>(S);
      auto LT = VerifyExpression(AS->GetLHS());
      auto RT = VerifyExpression(AS->GetRHS());
      if(LT.has_value() && RT.has_value()) {
        if(LT != RT) {
          ERROR("Type mismatch in assignment statement at line %d\n", S->GetLineNo());
          return false;
        }
      } else {
        return false;
      }
      break;
    }
    case ASTStatement::ST_IF: {
      auto *IS = static_cast<ASTStatementIf*>(S);
      auto ET = VerifyExpression(IS->GetCondition());
      if(ET.has_value()) {
        if(ET != TY_INTEGER) {
          ERROR("Invalid condition type in if statement at line %d\n", S->GetLineNo());
          return false;
        }
        if(!VerifyBlock(IS->GetStatements())) {
          return false;
        }
      } else {
        return false;
      }
      break;
    }
    case ASTStatement::ST_IFELSE: {
      auto *IES = static_cast<ASTStatementIfElse*>(S);
      auto ET = VerifyExpression(IES->GetCondition());
      if(ET.has_value()) {
        if(ET != TY_INTEGER) {
          ERROR("Invalid condition type in if-else statement at line %d\n", S->GetLineNo());
          return false;
        }
        if(!VerifyBlock(IES->GetIfStatements())) {
          return false;
        }
        if(!VerifyBlock(IES->GetElseStatements())) {
          return false;
        }
      } else {
        return false;
      }
      break;
    }
    case ASTStatement::ST_WHILE: {
      auto *WS = static_cast<ASTStatementWhile*>(S);
      auto ET = VerifyExpression(WS->GetCondition());
      if(ET.has_value()) {
        if(ET != TY_INTEGER) {
          ERROR("Invalid condition type in while statement at line %d\n", S->GetLineNo());
          return false;
        }

        // LIMIT: no nested loops
        std::function<bool(ASTStatement*)> HasNestedLoop = [&](ASTStatement* S) {
          bool Result = false; 
          if(S->GetType() == ASTStatement::ST_WHILE) {
            return true;
          }
          else if(S->GetType() == ASTStatement::ST_IF) {
            auto *IS = static_cast<ASTStatementIf*>(S);
            for(auto *ISS : IS->GetStatements()) {
              Result |= HasNestedLoop(ISS);
            }
          }
          else if(S->GetType() == ASTStatement::ST_IFELSE) {
            auto *IES = static_cast<ASTStatementIfElse*>(S);
            for(auto *ISS : IES->GetIfStatements()) {
              Result |= HasNestedLoop(ISS);
            }
            for(auto *ISS : IES->GetElseStatements()) {
              Result |= HasNestedLoop(ISS);
            }
          }
          return Result;
        };

        for(auto *SS : WS->GetStatements()) {
          if(HasNestedLoop(SS)) {
            ERROR("Nested loops are not supported at line %d\n", S->GetLineNo());
            return false;
          }
        }

        if(!VerifyBlock(WS->GetStatements())) {
          return false;
        }
      } else {
        return false;
      }
      break;
    }
    case ASTStatement::ST_RETURN: {
      auto *RS = static_cast<ASTStatementReturn*>(S);

      if(RS->GetReturnValue() != nullptr) {
        auto ET = VerifyExpression(RS->GetReturnValue());
        if(ET.has_value()) {
          if(ET != RS->GetParent()->GetReturnType()) {
            ERROR("Type mismatch in return statement at line %d\n", S->GetLineNo());
            return false;
          }
        } else if(!ET.has_value()) {
          return false;
        }
      } else {
        if(RS->GetParent()->GetReturnType() != TY_VOID) {
          ERROR("Return statement must have a value at line %d\n", S->GetLineNo());
          return false;
        }
      }
      break;
    }
    case ASTStatement::ST_CALL: {
      auto *CS = static_cast<ASTStatementFunctionCall*>(S);
      auto ET = VerifyExpression(CS->GetFunctionCall());
      if(!ET.has_value()) {
        return false;
      }
      break;
    }
  }
  return true;
}

static bool VerifyFunction(ASTFunction* F) {
  // function-level block must have return statement as the last statement
  if(F->GetStatements().size() == 0 || F->GetStatements().back()->GetType() != ASTStatement::ST_RETURN) {
    ERROR("Missing return statement for function %s\n", F->GetName());
    return false;
  }

  // variables cannot have void as type
  for(auto &Var : F->GetVariables()) {
    if(Var.second == TY_VOID) {
      ERROR("Variable %s has void as type\n", Var.first.c_str());
      return false;
    }
  }

  // LIMIT: at most 10 variables
  if(F->GetVariables().size() > 10) {
    ERROR("Too many variables in function %s\n", F->GetName());
    return false;
  }

  // LIMIT: at most 3 parameters
  if(F->GetParameters().size() > 3) {
    ERROR("Too many parameters in function %s\n", F->GetName());
    return false;
  }

  if(!VerifyBlock(F->GetStatements())) {
    return false;
  }
  return true;
}

bool IRGen::Verify() {
  bool HasEntry = false;
  std::set<std::string> FunctionNames;
  
  for(auto *F : Module_->GetFunctions()) {
    if(strcmp(F->GetName(), "main") == 0) {
      // check proto
      if(F->GetParameters().size() != 0 || F->GetReturnType() != TY_INTEGER) {
        ERROR("Invalid prototype for entry function main\n");
        return false;
      }
      HasEntry = true;
    }

    if(FunctionNames.count(F->GetName()) == 0) {
      FunctionNames.insert(F->GetName());
    } else {
      ERROR("Duplicate function name %s\n", F->GetName());
      return false;
    }
  }

  if(!HasEntry) {
    ERROR("Entry function main is not defined\n");
    return false;
  }



  for(auto *F : Module_->GetFunctions()) {
    if(!VerifyFunction(F)) {
      return false;
    }
  }
  return true;
}

std::pair<ModuleGenCtx, Module*> IRGen::Generate() {
  Module* TheModule = new Module("<main>");
  ModuleGenCtx Ctx(Module_);

  for(auto *F : Module_->GetFunctions()) {
    TheModule->AddFunction(GenerateFunction(Ctx, F));
  }
  return std::make_pair(Ctx, TheModule);
}

#define __ Ctx.B-> 

BasicBlock* IRGen::GenerateBlock(FuncGenCtx& Ctx, const std::vector<ASTStatement*>& Statements) {
  auto *B = __ CreateBlock();
  __ SetInsertionPoint(B);
  for(auto *S : Statements) {
    GenerateStatement(Ctx, S);
  }
  return B;
}

static bool BlockEndsWithReturn(const std::vector<ASTStatement*>& Statements) {
  if(Statements.size() == 0) {
    return false;
  }
  return Statements.back()->GetType() == ASTStatement::ST_RETURN;
}

Operand IRGen::GenerateExpression(FuncGenCtx& Ctx, ASTExpression* E, bool CallVoid) {
  switch(E->GetType()) {
    case ASTExpression::EX_INTEGER: {
      return __ Imm(static_cast<ASTExpressionInteger*>(E)->GetValue());
    }

    case ASTExpression::EX_STRING: {
      auto *Label = Ctx.MCtx->GetStringLiteral(static_cast<ASTExpressionString*>(E)->GetValue());
      auto RetVal = __ NewReg();
      __ LoadLabel(RetVal, Label);
      return RetVal;
    }

    case ASTExpression::EX_VARIABLE: {
      return Ctx.GetVariable(static_cast<ASTExpressionVariable*>(E)->GetName());
    }

    case ASTExpression::EX_ARRAY_ACCESS: {
      auto *AE = static_cast<ASTExpressionArrayAccess*>(E);
      auto &Name = AE->GetName();
      auto Array = Ctx.GetVariable(Name);
      auto Index = GenerateExpression(Ctx, AE->GetIndex());
      auto RetVal = __ NewReg();
      __ ArrayLoad(RetVal, Array, Index);
      return RetVal;
    }

    case ASTExpression::EX_FUNCTION_CALL: {
      auto *FCE = static_cast<ASTExpressionFunctionCall*>(E);
      auto RetVal = __ NewReg();
      
      std::vector<Operand> Args;
      for(auto *Arg : FCE->GetArguments()) {
        Args.push_back(GenerateExpression(Ctx, Arg));
      }

      if(CallVoid) {
        __ CallVoid(FCE->GetName().c_str(), Args);
      } else {
        __ Call(FCE->GetName().c_str(), RetVal, Args);
      }
      return RetVal;
    }
    
    case ASTExpression::EX_BINARY: {
      auto *BE = static_cast<ASTExpressionBinary*>(E);
      auto LHS = GenerateExpression(Ctx, BE->GetLHS());
      auto RHS = GenerateExpression(Ctx, BE->GetRHS());
      auto NewR = __ NewReg();

      switch(BE->GetOperator()) {
        // TODO: Support all arithmetics
        case ASTExpressionBinary::OP_ADD: {
          __ Add(NewR, LHS, RHS);
          break;
        }
        case ASTExpressionBinary::OP_SUB: {
          __ Sub(NewR, LHS, RHS);
          break;
        }
        case ASTExpressionBinary::OP_MUL: {
          __ Mul(NewR, LHS, RHS);
          break;
        }
        case ASTExpressionBinary::OP_DIV: {
          __ Div(NewR, LHS, RHS);
          break;
        }
        case ASTExpressionBinary::OP_LT: {
          __ Lt(NewR, LHS, RHS);
          break;
        }
        case ASTExpressionBinary::OP_GT: {
          __ Gt(NewR, LHS, RHS);
          break;
        }
        case ASTExpressionBinary::OP_LTE: {
          __ Le(NewR, LHS, RHS);
          break;
        }
        case ASTExpressionBinary::OP_GTE: {
          __ Ge(NewR, LHS, RHS);
          break;
        }
        case ASTExpressionBinary::OP_EQ: {
          __ Eq(NewR, LHS, RHS);
          break;
        }
        case ASTExpressionBinary::OP_NEQ: {
          __ Ne(NewR, LHS, RHS);
          break;
        }
        case ASTExpressionBinary::OP_AND: {
          __ And(NewR, LHS, RHS);
          break;
        }
        case ASTExpressionBinary::OP_OR: {
          __ Or(NewR, LHS, RHS);
          break;
        }
        case ASTExpressionBinary::OP_XOR: {
          __ Xor(NewR, LHS, RHS);
          break;
        }
        default: {
          assert(false && "unhandled binary operator");
        }
      }
      return NewR;
    }

    default: {
      assert(false && "unhandled expression type");
    }
  }
}

void IRGen::GenerateStatement(FuncGenCtx& Ctx, ASTStatement* S) {
  switch(S->GetType()) {
    case ASTStatement::ST_ASSIGN: {
      auto Op = GenerateExpression(Ctx, static_cast<ASTStatementAssign*>(S)->GetRHS());
      auto *LHS = static_cast<ASTStatementAssign*>(S)->GetLHS();
      if(LHS->GetType() == ASTExpression::EX_VARIABLE) {
        __ Assign(Ctx.GetVariable(static_cast<ASTExpressionVariable*>(LHS)->GetName()), Op);
      } else if(LHS->GetType() == ASTExpression::EX_ARRAY_ACCESS) {
        auto *AE = static_cast<ASTExpressionArrayAccess*>(LHS);
        auto Array = Ctx.GetVariable(AE->GetName());
        auto Index = GenerateExpression(Ctx, AE->GetIndex());
        __ ArrayStore(Array, Index, Op);
      }
      break;
    }
    case ASTStatement::ST_IF: {
      auto *IS = static_cast<ASTStatementIf*>(S);
      auto *NextB = __ CreateBlock();
      auto Cond = GenerateExpression(Ctx, IS->GetCondition());
      
      auto *Current = __ Current();
      auto *Then = GenerateBlock(Ctx, IS->GetStatements());
      if(!BlockEndsWithReturn(IS->GetStatements())) {
        __ Jmp(NextB);
      }
      __ SetInsertionPoint(Current);
      __ Jnz(Cond, Then, NextB);
      __ SetInsertionPoint(NextB);
      break;
    }
    case ASTStatement::ST_IFELSE: {
      auto *IES = static_cast<ASTStatementIfElse*>(S);
      auto *NextB = __ CreateBlock();
      auto Cond = GenerateExpression(Ctx, IES->GetCondition());
      
      auto *Current = __ Current();
      auto *Then = GenerateBlock(Ctx, IES->GetIfStatements());
      if(!BlockEndsWithReturn(IES->GetIfStatements())) {
        __ Jmp(NextB);
      }

      auto *Else = GenerateBlock(Ctx, IES->GetElseStatements());
      if(!BlockEndsWithReturn(IES->GetElseStatements())) {
        __ Jmp(NextB);
      }

      __ SetInsertionPoint(Current);
      __ Jnz(Cond, Then, Else);
      __ SetInsertionPoint(NextB);
      break;
    }
    case ASTStatement::ST_RETURN: {
      auto *RS = static_cast<ASTStatementReturn*>(S);
      auto *RetVal = RS->GetReturnValue();
      if(RetVal != nullptr) {
        __ Ret(GenerateExpression(Ctx, RetVal));
      } else {
        __ RetVoid();
      }
      break;
    }
    case ASTStatement::ST_WHILE: {
      auto *WS = static_cast<ASTStatementWhile*>(S);
      auto *Current = __ Current();
      auto *NextB = __ CreateBlock();

      auto *LoopB = GenerateBlock(Ctx, WS->GetStatements());
      auto Cond = GenerateExpression(Ctx, WS->GetCondition());
      __ Jnz(Cond, LoopB, NextB);

      __ SetInsertionPoint(Current);
      __ Jmp(LoopB);

      __ SetInsertionPoint(NextB);
      break;
    }
    case ASTStatement::ST_CALL: {
      auto *CS = static_cast<ASTStatementFunctionCall*>(S);
      GenerateExpression(Ctx, CS->GetFunctionCall(), true);
      break;
    }
    default: {
      assert(false && "unhandled statement type");
    }
  };
}

#undef __

Function* IRGen::GenerateFunction(ModuleGenCtx& MCtx, ASTFunction* F) {
  FuncBuilder* B = new FuncBuilder(F->GetName(), F->GetParameters().size());

  FuncGenCtx Ctx(F, B, &MCtx);
  Ctx.InitVariables();
  GenerateBlock(Ctx, F->GetStatements());

  // add initializers
  auto *Entry = B->GetFunction()->Entry();
  auto *Inst = Entry->Head();
  for(auto &Var : F->GetVariables()) {
    Entry->InsertBefore(new AssignInst(Ctx.GetVariable(Var.first), B->Imm(0)), Inst);
  }
  return B->GetFunction();
}

} // namespace klang