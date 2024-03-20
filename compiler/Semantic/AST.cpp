#include <Semantic/AST.h>
#include <Logging.h>

#include <iostream>
#include <cassert>
#include <cstring>

namespace klang {

ASTModule::ASTModule(const std::vector<ASTFunction*>& Functions) : Functions_(Functions) {
  for(auto Function : Functions_) {
    Function->SetParent(this);
  }
}

ASTModule::~ASTModule() {
  for (auto Function : Functions_) {
    delete Function;
  }
}

std::optional<ASTFuncPrototype> ASTModule::GetPrototype(const char* Name) const  {
  auto It = ExternalFunctions_.find(Name);
  if(It != ExternalFunctions_.end()) {
    return It->second;
  }

  for(auto *F : Functions_) {
    if(std::strcmp(F->GetName(), Name) == 0) {
      return F->GetPrototype();
    }
  }
  return std::nullopt;
}

void ASTModule::Print() const {
  for(auto Function : Functions_) {
    Function->Print();
  }
}

ASTFunction::ASTFunction(const char* Name, ASTType ReturnType, 
    const std::vector<ASTParameter>& Parameters, const std::vector<ASTVarDef>& Variables, const std::vector<ASTStatement*>& Statements)
    : Name_(Name), ReturnType_(ReturnType), Parameters_(Parameters), Variables_(Variables), Statements_(Statements) {
  for(auto Statement : Statements_) {
    Statement->SetParent(this);
  }
}

ASTFunction::~ASTFunction() {
  for (auto Statement : Statements_) {
    delete Statement;
  }
}

static const char* GetTypeName(ASTType Type) {
  switch(Type) {
    case TY_INTEGER: return "int";
    case TY_ARRAY: return "array";
    case TY_STRING: return "string";
    case TY_VOID: return "void";
    default: assert(false && "Invalid type");
  }
  return nullptr;
}

void ASTFunction::Print() const {
  IndentPrinter OS(std::cout);

  OS.DoIndent();
  OS << "function " << Name_ << "(";
  for(size_t i = 0; i < Parameters_.size(); i++) {
    OS << GetTypeName(Parameters_[i].second) << " " << Parameters_[i].second;
    if(i != Parameters_.size() - 1) {
      OS << ", ";
    }
  }
  OS << ") : ";
  for(size_t i = 0; i < Variables_.size(); i++) {
    OS << GetTypeName(Variables_[i].second) << " " << Variables_[i].first;
    if(i != Variables_.size() - 1) {
      OS << ", ";
    }
  }
  OS << " -> " << GetTypeName(ReturnType_) << " {\n";

  OS.Indent();
  for(auto Statement : Statements_) {
    Statement->Print(OS);
  }
  OS.Unindent();

  OS.DoIndent();
  OS << "}\n";
}

ASTStatementAssign::ASTStatementAssign(ASTExpression* LHS, ASTExpression* RHS, int LineNo) : ASTStatement(ST_ASSIGN, LineNo), LHS_(LHS), RHS_(RHS) {
  LHS_->SetParent(this);
  RHS_->SetParent(this);
}

ASTStatementAssign::~ASTStatementAssign() {
  delete LHS_;
  delete RHS_;
}

void ASTStatementAssign::Print(IndentPrinter& OS) const {
  OS.DoIndent();
  LHS_->Print(OS);
  OS << " = ";
  RHS_->Print(OS);
  OS << ";\n";
}

ASTStatementIf::ASTStatementIf(ASTExpression* Condition, const std::vector<ASTStatement*>& Statements, int LineNo)
    : ASTStatement(ST_IF, LineNo), Condition_(Condition), Statements_(Statements) {
  Condition_->SetParent(this);
}

ASTStatementIf::~ASTStatementIf() {
  delete Condition_;
  for(auto Statement : Statements_) {
    delete Statement;
  }
}

void ASTStatementIf::Print(IndentPrinter& OS) const {
  OS.DoIndent();
  OS << "if (";
  Condition_->Print(OS);
  OS << ") {\n";

  OS.Indent();
  for(auto Statement : Statements_) {
    Statement->Print(OS);
  }
  OS.Unindent();

  OS.DoIndent();
  OS << "};\n";
}

ASTStatementIfElse::ASTStatementIfElse(ASTExpression* Condition, const std::vector<ASTStatement*>& IfStatements,
                     const std::vector<ASTStatement*>& ElseStatements, int LineNo)
    : ASTStatement(ST_IFELSE, LineNo), Condition_(Condition), IfStatements_(IfStatements), ElseStatements_(ElseStatements) {
  Condition_->SetParent(this);
}

ASTStatementIfElse::~ASTStatementIfElse() {
  delete Condition_;
  for(auto IfStatement : IfStatements_) {
    delete IfStatement;
  }
  for(auto ElseStatement : ElseStatements_) {
    delete ElseStatement;
  }
}

void ASTStatementIfElse::Print(IndentPrinter& OS) const {
  OS.DoIndent();
  OS << "if (";
  Condition_->Print(OS);
  OS << ") {\n";
  OS.Indent();
  for(auto Statement : IfStatements_) {
    Statement->Print(OS);
  }
  OS.Unindent();
  OS.DoIndent();
  OS << "} else {\n";
  OS.Indent();
  for(auto Statement : ElseStatements_) {
    Statement->Print(OS);
  }
  OS.Unindent();
  OS.DoIndent();
  OS << "}\n";
}

ASTStatementWhile::ASTStatementWhile(ASTExpression* Condition, const std::vector<ASTStatement*>& Statements, int LineNo)
    : ASTStatement(ST_WHILE, LineNo), Condition_(Condition), Statements_(Statements) {
  Condition_->SetParent(this);
}

ASTStatementWhile::~ASTStatementWhile() {
  delete Condition_;
  for(auto Statement : Statements_) {
    delete Statement;
  }
}

void ASTStatementWhile::Print(IndentPrinter& OS) const {
  OS.DoIndent();
  OS << "while (";
  Condition_->Print(OS);
  OS << ") {\n";
  OS.Indent();
  for(auto Statement : Statements_) {
    Statement->Print(OS);
  }
  OS.Unindent();
  OS << "}\n";
}

ASTStatementReturn::ASTStatementReturn(ASTExpression* ReturnValue, int LineNo)
    : ASTStatement(ST_RETURN, LineNo), ReturnValue_(ReturnValue) {
  ReturnValue_->SetParent(this);
}

ASTStatementReturn::~ASTStatementReturn() {
  delete ReturnValue_;
}

void ASTStatementReturn::Print(IndentPrinter& OS) const {
  OS.DoIndent();
  OS << "return";
  if(ReturnValue_) {
    OS << " ";
    ReturnValue_->Print(OS);
  }
  OS << ";\n";
}

ASTStatementFunctionCall::ASTStatementFunctionCall(ASTExpression* FunctionCall, int LineNo)
    : ASTStatement(ST_CALL, LineNo), FunctionCall_(FunctionCall) {
  FunctionCall_->SetParent(this);
}

ASTStatementFunctionCall::~ASTStatementFunctionCall() {
  delete FunctionCall_;
}

void ASTStatementFunctionCall::Print(IndentPrinter& OS) const {
  OS.DoIndent();
  FunctionCall_->Print(OS);
  OS << ";\n";
}

void ASTExpressionInteger::Print(IndentPrinter& OS) const {
  OS << Value_;
}

void ASTExpressionString::Print(IndentPrinter& OS) const {
  OS << "\"" << Escape(Value_) << "\"";
}

ASTExpressionBinary::~ASTExpressionBinary() {
  delete LHS_;
  delete RHS_;
}

void ASTExpressionBinary::Print(IndentPrinter& OS) const {
  LHS_->Print(OS);
  switch(Operator_) {
    case OP_ADD: OS << " + "; break;
    case OP_SUB: OS << " - "; break;
    case OP_MUL: OS << " * "; break;
    case OP_DIV: OS << " / "; break;
    case OP_MOD: OS << " % "; break;
    case OP_EQ: OS << " == "; break;
    case OP_NEQ: OS << " != "; break;
    case OP_LT: OS << " < "; break;
    case OP_LTE: OS << " <= "; break;
    case OP_GT: OS << " > "; break;
    case OP_GTE: OS << " >= "; break;
    case OP_AND: OS << " && "; break;
    case OP_OR: OS << " || "; break;
    default: assert(false && "Invalid binary operator");
  }
  RHS_->Print(OS);
}

ASTExpressionFunctionCall::~ASTExpressionFunctionCall() {
  for(auto Argument : Arguments_) {
    delete Argument;
  }
}

void ASTExpressionFunctionCall::Print(IndentPrinter& OS) const {
  OS << Name_ << "(";
  for(size_t i = 0; i < Arguments_.size(); i++) {
    Arguments_[i]->Print(OS);
    if(i != Arguments_.size() - 1) {
      OS << ", ";
    }
  }
  OS << ")";
}

void ASTExpressionVariable::Print(IndentPrinter& OS) const {
  OS << Name_;
}

ASTExpressionArrayAccess::~ASTExpressionArrayAccess() {
  delete Index_;
}

void ASTExpressionArrayAccess::Print(IndentPrinter& OS) const {
  OS << Name_ << "[";
  Index_->Print(OS);
  OS << "]";
}

} // namespace klang