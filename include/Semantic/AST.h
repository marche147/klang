#ifndef _AST_H
#define _AST_H

#include <string>
#include <algorithm>
#include <vector>
#include <map>
#include <unordered_map>

#include <Util.h>

namespace klang {

class ASTModule;
class ASTFunction;
class ASTStatement;
class ASTExpression;

#define TY_INTEGER 0
#define TY_ARRAY 1
#define TY_STRING 2
#define TY_VOID 3

using ASTType = int;
using ASTName = std::string;
using ASTParameter = std::pair<ASTName, ASTType>;
using ASTVarDef = std::pair<ASTName, ASTType>;
using ASTFuncPrototype = std::pair<ASTType, std::vector<ASTType>>;

class ASTModule {
public:
  ASTModule(const std::vector<ASTFunction*>& Functions);
  ~ASTModule();

  const std::vector<ASTFunction*>& GetFunctions() const { return Functions_; }

  void AddExternalFunction(const char* Name, ASTFuncPrototype Prototype) {
    ExternalFunctions_[Name] = Prototype;
  }

  std::optional<ASTFuncPrototype> GetPrototype(const char* Name) const;
  
  void Print() const; 

private:
  std::vector<ASTFunction*> Functions_;
  std::map<ASTName, ASTFuncPrototype> ExternalFunctions_;
};

class ASTFunction {
public:
  ASTFunction(const char* Name, ASTType ReturnType, 
    const std::vector<ASTParameter>& Parameters, const std::vector<ASTVarDef>& Variables, const std::vector<ASTStatement*>& Statements);
  ~ASTFunction();

  const char* GetName() const { return Name_.c_str(); }
  ASTModule* GetParent() const { return Parent_; }
  ASTType GetReturnType() const { return ReturnType_; }
  const std::vector<ASTParameter>& GetParameters() const { return Parameters_; }
  const std::vector<ASTVarDef>& GetVariables() const { return Variables_; }
  const std::vector<ASTStatement*>& GetStatements() const { return Statements_; }

  ASTFuncPrototype GetPrototype() const {
    std::vector<ASTType> ParamTypes;
    std::transform(Parameters_.begin(), Parameters_.end(), std::back_inserter(ParamTypes), [](const ASTParameter& P) { return P.second; });
    return std::make_pair(ReturnType_, ParamTypes);
  }

  void Print() const;

protected:
  friend class ASTModule;

  void SetParent(ASTModule* Parent) { Parent_ = Parent; }

private:
  ASTName Name_;
  ASTModule* Parent_;
  ASTType ReturnType_;
  std::vector<ASTParameter> Parameters_;
  std::vector<ASTVarDef> Variables_;
  std::vector<ASTStatement*> Statements_;
};

class ASTStatement {
public:
  enum ASTStatementType : int {
    ST_ASSIGN,
    ST_IF,
    ST_IFELSE,
    ST_WHILE,
    ST_RETURN,
    ST_CALL,
  };

  ASTStatement(ASTStatementType Type, int LineNo) : Type_(Type), Parent_(nullptr), LineNo_(LineNo) {}
  virtual ~ASTStatement() {}

  ASTFunction* GetParent() const { return Parent_; }
  ASTStatementType GetType() const { return Type_; }
  int GetLineNo() const { return LineNo_; }

  virtual void Print(IndentPrinter& OS) const = 0;
  
  virtual void SetParent(ASTFunction* Parent) { Parent_ = Parent; }

private:
  ASTFunction* Parent_;
  ASTStatementType Type_;
  int LineNo_;
};

class ASTStatementAssign : public ASTStatement {
public:
  ASTStatementAssign(ASTExpression* LHS, ASTExpression* RHS, int LineNo);
  ~ASTStatementAssign();

  ASTExpression* GetLHS() const { return LHS_; }
  ASTExpression* GetRHS() const { return RHS_; }

  virtual void Print(IndentPrinter& OS) const override;

private:
  ASTExpression* LHS_;
  ASTExpression* RHS_;
};

class ASTStatementIf : public ASTStatement {
public:
  ASTStatementIf(ASTExpression* Condition, const std::vector<ASTStatement*>& Statements, int LineNo);
  ~ASTStatementIf();

  ASTExpression* GetCondition() const { return Condition_; }
  const std::vector<ASTStatement*>& GetStatements() const { return Statements_; }

  virtual void Print(IndentPrinter& OS) const override;

  virtual void SetParent(ASTFunction* Parent) override {
    ASTStatement::SetParent(Parent);
    for(auto Statement : Statements_) {
      Statement->SetParent(Parent);
    }
  }

private:
  ASTExpression* Condition_;
  std::vector<ASTStatement*> Statements_;
};

class ASTStatementIfElse : public ASTStatement {
public:
  ASTStatementIfElse(ASTExpression* Condition, const std::vector<ASTStatement*>& IfStatements,
                     const std::vector<ASTStatement*>& ElseStatements, int LineNo);
  ~ASTStatementIfElse();

  ASTExpression* GetCondition() const { return Condition_; }
  const std::vector<ASTStatement*>& GetIfStatements() const { return IfStatements_; }
  const std::vector<ASTStatement*>& GetElseStatements() const { return ElseStatements_; }

  virtual void Print(IndentPrinter& OS) const override;

  virtual void SetParent(ASTFunction* Parent) override {
    ASTStatement::SetParent(Parent);
    for(auto Statement : IfStatements_) {
      Statement->SetParent(Parent);
    }
    for(auto Statement : ElseStatements_) {
      Statement->SetParent(Parent);
    }
  }

private:
  ASTExpression* Condition_;
  std::vector<ASTStatement*> IfStatements_;
  std::vector<ASTStatement*> ElseStatements_;
};

class ASTStatementWhile : public ASTStatement {
public:
  ASTStatementWhile(ASTExpression* Condition, const std::vector<ASTStatement*>& Statements, int LineNo);
  ~ASTStatementWhile();

  ASTExpression* GetCondition() const { return Condition_; }
  const std::vector<ASTStatement*>& GetStatements() const { return Statements_; }

  virtual void Print(IndentPrinter& OS) const override;

  virtual void SetParent(ASTFunction* Parent) override {
    ASTStatement::SetParent(Parent);
    for(auto Statement : Statements_) {
      Statement->SetParent(Parent);
    }
  }

private:
  ASTExpression* Condition_;
  std::vector<ASTStatement*> Statements_;
};

class ASTStatementReturn : public ASTStatement {  
public:
  ASTStatementReturn(ASTExpression* ReturnValue, int LineNo);
  ~ASTStatementReturn();

  ASTExpression* GetReturnValue() const { return ReturnValue_; }

  virtual void Print(IndentPrinter& OS) const override;

private:
  ASTExpression* ReturnValue_;
};

class ASTStatementFunctionCall : public ASTStatement {
public:
  ASTStatementFunctionCall(ASTExpression* FunctionCall, int LineNo);
  ~ASTStatementFunctionCall();

  ASTExpression* GetFunctionCall() const { return FunctionCall_; }

  virtual void Print(IndentPrinter& OS) const override;

public:
  ASTExpression* FunctionCall_;
};

class ASTExpression {
public:
  enum ASTExpressionType : int {
    EX_INTEGER,
    EX_STRING, 

    EX_BINARY,
    
    EX_FUNCTION_CALL,

    EX_VARIABLE,
    EX_ARRAY_ACCESS,
  };
  virtual ~ASTExpression() {}

  ASTExpression(ASTExpressionType Type) : Type_(Type), Parent_(nullptr) {}

  ASTExpressionType GetType() const { return Type_; }
  ASTStatement* GetParent() const { return Parent_; }
  virtual void SetParent(ASTStatement* Parent) { Parent_ = Parent; }  

  virtual void Print(IndentPrinter& OS) const = 0;

private:
  ASTExpressionType Type_;
  ASTStatement* Parent_;
};

class ASTExpressionInteger : public ASTExpression {
public:
  ASTExpressionInteger(int64_t Value) : ASTExpression(EX_INTEGER), Value_(Value) {}

  int64_t GetValue() const { return Value_; }

  virtual void Print(IndentPrinter& OS) const override;

private:
  int64_t Value_;
};

class ASTExpressionString : public ASTExpression {
public:
  ASTExpressionString(const std::string& Value) : ASTExpression(EX_STRING), Value_(Value) {}

  const std::string& GetValue() const { return Value_; }

  virtual void Print(IndentPrinter& OS) const override;

private:
  std::string Value_;
};

class ASTExpressionBinary : public ASTExpression {
public:
  enum ASTBinaryOperator : int {
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_EQ,
    OP_NEQ,
    OP_LT,
    OP_GT,
    OP_LTE,
    OP_GTE,
    OP_AND,
    OP_OR,
    OP_XOR,
  };

  ASTExpressionBinary(ASTBinaryOperator Operator, ASTExpression* LHS, ASTExpression* RHS)
    : ASTExpression(EX_BINARY), Operator_(Operator), LHS_(LHS), RHS_(RHS) { }
  ~ASTExpressionBinary();

  ASTBinaryOperator GetOperator() const { return Operator_; }
  ASTExpression* GetLHS() const { return LHS_; }
  ASTExpression* GetRHS() const { return RHS_; }

  virtual void SetParent(ASTStatement* Parent) override {
    ASTExpression::SetParent(Parent);
    LHS_->SetParent(Parent);
    RHS_->SetParent(Parent);
  }

  virtual void Print(IndentPrinter& OS) const override;

private:
  ASTBinaryOperator Operator_;
  ASTExpression* LHS_;
  ASTExpression* RHS_;
};

class ASTExpressionFunctionCall : public ASTExpression {
public:
  ASTExpressionFunctionCall(const ASTName& Name, const std::vector<ASTExpression*>& Arguments)
    : ASTExpression(EX_FUNCTION_CALL), Name_(Name), Arguments_(Arguments) {}
  ~ASTExpressionFunctionCall();

  const ASTName& GetName() const { return Name_; }
  const std::vector<ASTExpression*>& GetArguments() const { return Arguments_; }

  virtual void Print(IndentPrinter& OS) const override;

  virtual void SetParent(ASTStatement* Parent) override {
    ASTExpression::SetParent(Parent);
    for(auto Argument : Arguments_) {
      Argument->SetParent(Parent);
    }
  }

private:
  ASTName Name_;
  std::vector<ASTExpression*> Arguments_;
};

class ASTExpressionVariable : public ASTExpression {
public:
  ASTExpressionVariable(const ASTName& Name) : ASTExpression(EX_VARIABLE), Name_(Name) {}

  const ASTName& GetName() const { return Name_; }

  virtual void Print(IndentPrinter& OS) const override;

private:
  ASTName Name_;
};

class ASTExpressionArrayAccess : public ASTExpression {
public:
  ASTExpressionArrayAccess(const ASTName& Name, ASTExpression* Index)
    : ASTExpression(EX_ARRAY_ACCESS), Name_(Name), Index_(Index) {}
  ~ASTExpressionArrayAccess();

  const ASTName& GetName() const { return Name_; }
  ASTExpression* GetIndex() const { return Index_; }

  virtual void Print(IndentPrinter& OS) const override;

  virtual void SetParent(ASTStatement* Parent) override {
    ASTExpression::SetParent(Parent);
    Index_->SetParent(Parent);
  }

private:
  ASTName Name_;
  ASTExpression* Index_;
};

}

#endif