#ifndef _IR_H
#define _IR_H

#include <vector>
#include <set>
#include <string>

#include <cstdint>
#include <cassert>

namespace klang {

class Module;
class Function;
class BasicBlock;
class Instruction;
class Operand;

class Interpreter;

class Module {
public:
  Module(const char* Name) : Name_(Name) {}

  void AddFunction(Function* Func);

  std::vector<Function*>::iterator begin() { return Functions_.begin(); }
  std::vector<Function*>::iterator end() { return Functions_.end(); }
  std::vector<Function*>::const_iterator begin() const { return Functions_.begin(); }
  std::vector<Function*>::const_iterator end() const { return Functions_.end(); }

private:
  std::string Name_; 
  std::vector<Function*> Functions_;
};

class Function {
public:
  Function(const char* Name, size_t NumParams) : Name_(Name), NumParams_(NumParams), NumRegs_(0), Parent_(nullptr) {}

  ~Function();

  Module* Parent() const { return Parent_; }
  std::string Name() const { return Name_; }
  size_t Size() const { return BasicBlocks_.size(); }
  size_t NumParams() const { return NumParams_; }
  size_t NumRegs() const { return NumRegs_; }

  size_t NewReg() { return NumRegs_++; }

  std::vector<BasicBlock*>::iterator begin() { return BasicBlocks_.begin(); }
  std::vector<BasicBlock*>::iterator end() { return BasicBlocks_.end(); }

  std::vector<BasicBlock*> PostOrder() const;
  BasicBlock* Entry() const { return BasicBlocks_[0]; }

  void AddBasicBlock(BasicBlock* BB);
  BasicBlock* Remove(BasicBlock* BB);

  void Print() const;

protected: 
  friend class Module;
  void SetParent(Module* Parent) { Parent_ = Parent; }

private:
  void PostOrderImpl(BasicBlock* Current, std::set<BasicBlock*> Visited, std::vector<BasicBlock*>& PostOrder) const;

  std::string Name_;
  Module* Parent_;
  size_t NumParams_, NumRegs_;
  std::vector<BasicBlock*> BasicBlocks_;
};

class BasicBlock {
public:
  BasicBlock() : Parent_(nullptr), Index_(0), Size_(0), Head_(nullptr), Tail_(nullptr) {}

  ~BasicBlock();

  Function* Parent() const { return Parent_; }
  size_t Size() const { return Size_; }
  size_t Index() const { return Index_; }

  std::vector<BasicBlock*> Successors() const;
  std::vector<BasicBlock*> Predecessors() const;

  Instruction* Head() const { return Head_; }
  Instruction* Tail() const { return Tail_; }

  bool IsExit() const;

  void AddInstruction(Instruction* Inst);

  void InsertAfter(Instruction* Inst, Instruction* After);
  void InsertBefore(Instruction* Inst, Instruction* Before);
  Instruction* Replace(Instruction* Inst, Instruction* Replaced);
  Instruction* Remove(Instruction* Inst);

  void Print() const;

  class Iterator {
  public:
    Iterator(Instruction* Inst, bool Reverse);

    Instruction& operator*() const { return *Current_; }
    Instruction* operator->() const { return Current_; }

    Iterator& operator++();
    Iterator operator++(int);

    bool operator==(const Iterator& Other) const { return Current_ == Other.Current_; }
    bool operator!=(const Iterator& Other) const { return Current_ != Other.Current_; }

  private:
    void UpdateLinks();

    Instruction* Current_, *Next_, *Prev_;
    bool Reverse_;
  };

  Iterator begin() { return Iterator(Head_, false); }
  Iterator end() { return Iterator(nullptr, false); }

  Iterator rbegin() { return Iterator(Tail_, true); } 
  Iterator rend() { return Iterator(nullptr, true); }

protected:
  friend class Function;
  friend class Interpreter;

  void SetParent(Function* Parent) { Parent_ = Parent; }
  void SetIndex(size_t Index) { Index_ = Index; }

private:
  size_t Index_;
  Function* Parent_;
  size_t Size_;
  Instruction* Head_, *Tail_;
};

class Instruction {
public:
  enum InstructionType {
    Nop,
    Assign,
    Binary,

    Jmp,
    Jnz,

    Call,
    CallVoid,

    Ret,
    RetVoid,

    ArrayNew,
    ArrayLoad,
    ArrayStore,

    LoadLabel,
  };

  Instruction(InstructionType Type) : Type_(Type), Parent_(nullptr), Next_(nullptr), Prev_(nullptr) {}

  virtual ~Instruction(); 

  BasicBlock* Parent() const { return Parent_; }
  size_t Size() const { return Operands_.size(); }
  InstructionType Type() const { return Type_; }

  Operand GetOperand(size_t Id) const;

  virtual bool IsTerminator() const = 0;
  virtual bool HasSideEffects() const = 0;
  virtual size_t NumSuccessor() const = 0;
  virtual BasicBlock* Successor(size_t Id) const = 0;
  virtual bool Verify() const = 0;

  virtual size_t Ins() const = 0;
  virtual size_t Outs() const = 0;

  virtual Operand GetIn(size_t Id) const = 0;
  virtual Operand GetOut(size_t Id) const = 0;

  virtual void ReplaceIn(size_t Id, const Operand& New) = 0;
  virtual void ReplaceOut(size_t Id, const Operand& New) = 0;

  virtual void Print() const = 0;

protected:
  friend class BasicBlock;
  void SetParent(BasicBlock* Parent) { Parent_ = Parent; }
  void SetNext(Instruction* Next) { Next_ = Next; }
  void SetPrev(Instruction* Prev) { Prev_ = Prev; }
  void AddOperand(const Operand& Op) { Operands_.push_back(Op); }
  void SetOperand(size_t Id, const Operand& Op);

  Instruction* Next() const { return Next_; }
  Instruction* Prev() const { return Prev_; }

private:
  InstructionType Type_;
  BasicBlock* Parent_;
  Instruction* Next_, *Prev_;
  std::vector<Operand> Operands_;
};

class Operand {
public:
  enum Type : int {
    Register,
    Immediate,
    Parameter,
  };

  Operand() : Type_(Register), U_() {}
  Operand(const Operand& Other) = default;

  static Operand CreateRegister(size_t RegId) { 
    Operand Op;
    Op.Type_ = Register;
    Op.U_.RegId = RegId;
    return Op;
  }

  static Operand CreateImmediate(int64_t Imm) { 
    Operand Op;
    Op.Type_ = Immediate;
    Op.U_.Imm = Imm;
    return Op;
  }

  static Operand CreateParameter(size_t Param) { 
    Operand Op; 
    Op.Type_ = Parameter;
    Op.U_.Param = Param;
    return Op;
  }

  void Print() const;

  bool IsRegister() const { return Type_ == Register; }
  bool IsImmediate() const { return Type_ == Immediate; }
  bool IsParameter() const { return Type_ == Parameter; }

  size_t RegId() const { 
    assert(Type_ == Register && "Operand is not a register");
    return U_.RegId; 
  }
  
  int64_t Imm() const { 
    assert(Type_ == Immediate && "Operand is not an immediate");
    return U_.Imm; 
  }

  size_t Param() const { 
    assert(Type_ == Parameter && "Operand is not a parameter");
    return U_.Param; 
  }

  bool operator==(const Operand& Other) const {
    if(Type_ != Other.Type_) {
      return false;
    }
    if(Type_ == Register) {
      return U_.RegId == Other.U_.RegId;
    }
    if(Type_ == Immediate) {
      return U_.Imm == Other.U_.Imm;
    }
    return U_.Param == Other.U_.Param;
  }

  bool operator!=(const Operand& Other) const {
    return !(*this == Other);
  }

  bool operator<(const Operand& Other) const {
    if(Type_ != Other.Type_) {
      return Type_ < Other.Type_;
    }
    if(Type_ == Register) {
      return U_.RegId < Other.U_.RegId;
    }
    if(Type_ == Immediate) {
      return U_.Imm < Other.U_.Imm;
    }
    return U_.Param < Other.U_.Param;
  }

private:
  Type Type_;
  union {
    size_t RegId;
    size_t Param;
    int64_t Imm;
  } U_;
};

#pragma region Instructions
/// Instructions 
#define NORMAL_INST(NumOperands) \
  virtual bool IsTerminator() const override { return false; } \
  virtual size_t NumSuccessor() const override { return 0; } \
  virtual BasicBlock* Successor(size_t Id) const override { \
    assert(false && "Invalid successor id"); \
    return nullptr; \
  } \
  virtual bool Verify() const override { return Size() == (NumOperands); }

#define TERMINATOR_INST(NumOperands, NumSuccessors) \
public: \
  virtual bool IsTerminator() const override { return true; } \
  virtual size_t NumSuccessor() const override { return (NumSuccessors); } \
  virtual BasicBlock* Successor(size_t Id) const override { \
    assert(Id < Successors_.size() && "Invalid successor id"); \
    return Successors_[Id]; \
  } \
  virtual bool Verify() const override { return Size() == (NumOperands) && Successors_.size() == (NumSuccessors); } \
private: \
  std::vector<BasicBlock*> Successors_;

#define NO_INOUT() \
public: \
  virtual size_t Ins() const override { return 0; } \
  virtual size_t Outs() const override { return 0; } \
  virtual Operand GetIn(size_t Id) const override { \
    assert(false && "Invalid input id"); \
    return Operand(); \
  } \
  virtual Operand GetOut(size_t Id) const override { \
    assert(false && "Invalid output id"); \
    return Operand(); \
  } \
  virtual void ReplaceIn(size_t Id, const Operand& New) override { \
    assert(false && "Invalid input id"); \
  } \
  virtual void ReplaceOut(size_t Id, const Operand& New) override { \
    assert(false && "Invalid output id"); \
  }

class NopInst : public Instruction {
public:
  NopInst() : Instruction(Nop) {}

  NORMAL_INST(0);

  NO_INOUT();

  virtual bool HasSideEffects() const override { return false; }

  virtual void Print() const override;
};

class AssignInst : public Instruction {
public:
  AssignInst(const Operand& LHS, const Operand& RHS) : Instruction(Assign) {
    AddOperand(LHS);
    AddOperand(RHS);
  }

  NORMAL_INST(2);

  virtual bool HasSideEffects() const override { return false; }

  virtual size_t Ins() const override { return 1; }
  virtual size_t Outs() const override { return 1; }

  virtual Operand GetIn(size_t Id) const override { 
    assert(Id == 0 && "Invalid input id");
    return GetOperand(1); 
  }
  virtual Operand GetOut(size_t Id) const override { 
    assert(Id == 0 && "Invalid output id");
    return GetOperand(0); 
  }

  virtual void ReplaceIn(size_t Id, const Operand& New) override { 
    assert(Id == 0 && "Invalid input id");
    SetOperand(1, New);
  }
  virtual void ReplaceOut(size_t Id, const Operand& New) override { 
    assert(Id == 0 && "Invalid output id");
    SetOperand(0, New);
  }

  virtual void Print() const override;
};

class BinaryInst : public Instruction {
public:
  enum Operation : size_t {
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    And,
    Or,
    Xor,
    Shl,
    Shr,

    Lt, 
    Le, 
    Gt, 
    Ge,
    Eq,
    Ne, 
  };

  BinaryInst(Operation Op, const Operand& LHS, const Operand& RHS1, const Operand& RHS2) : Operation_(Op), Instruction(Binary) {
    AddOperand(LHS);
    AddOperand(RHS1);
    AddOperand(RHS2);
  }

  NORMAL_INST(3);

public:
  virtual bool HasSideEffects() const override { return false; }
  virtual size_t Ins() const override { return 2; }
  virtual size_t Outs() const override { return 1; }

  virtual Operand GetIn(size_t Id) const override { 
    assert(Id < 2 && "Invalid input id");
    return GetOperand(Id + 1); 
  }
  virtual Operand GetOut(size_t Id) const override { 
    assert(Id == 0 && "Invalid output id");
    return GetOperand(0); 
  }

  virtual void ReplaceIn(size_t Id, const Operand& New) override { 
    assert(Id < 2 && "Invalid input id");
    SetOperand(Id + 1, New);
  }
  virtual void ReplaceOut(size_t Id, const Operand& New) override { 
    assert(Id == 0 && "Invalid output id");
    SetOperand(0, New);
  }

  void Print() const override;

public:
  Operation GetOperation() const { return Operation_; }
  static int64_t Evaluate(Operation Op, int64_t Op1, int64_t Op2);

private:
  Operation Operation_;
};

class JmpInst : public Instruction {
public:
  JmpInst(BasicBlock* Target) : Instruction(Jmp) {
    Successors_.push_back(Target);
  }

  TERMINATOR_INST(0, 1);

  NO_INOUT();

  bool HasSideEffects() const override { return true; }

  void Print() const override;
};

class JnzInst : public Instruction {
public:
  JnzInst(const Operand& Cond, BasicBlock* True, BasicBlock* False) : Instruction(Jnz) {
    AddOperand(Cond);
    Successors_.push_back(True);
    Successors_.push_back(False);
  }

  bool HasSideEffects() const override { return true; }

  TERMINATOR_INST(1, 2);

public:
  virtual size_t Ins() const override { return 1; }
  virtual size_t Outs() const override { return 0; }

  virtual Operand GetIn(size_t Id) const override { 
    assert(Id == 0 && "Invalid input id");
    return GetOperand(0); 
  }
  virtual Operand GetOut(size_t Id) const override { 
    assert(false && "Invalid output id");
    return Operand();
  }

  virtual void ReplaceIn(size_t Id, const Operand& New) override { 
    assert(Id == 0 && "Invalid input id");
    SetOperand(0, New);
  }
  virtual void ReplaceOut(size_t Id, const Operand& New) override { 
    assert(false && "Invalid output id");
  }

public:
  void Print() const override;
};

class RetInst : public Instruction {
public:
  RetInst(const Operand& RetVal) : Instruction(Ret) {
    AddOperand(RetVal);
  }

  TERMINATOR_INST(1, 0);

public:
  bool HasSideEffects() const override { return true; }

  virtual size_t Ins() const override { return 1; }
  virtual size_t Outs() const override { return 0; }

  virtual Operand GetIn(size_t Id) const override { 
    assert(Id == 0 && "Invalid input id");
    return GetOperand(0); 
  }
  virtual Operand GetOut(size_t Id) const override { 
    assert(false && "Invalid output id");
    return Operand();
  }

  virtual void ReplaceIn(size_t Id, const Operand& New) override { 
    assert(Id == 0 && "Invalid input id");
    SetOperand(0, New);
  }
  virtual void ReplaceOut(size_t Id, const Operand& New) override { 
    assert(false && "Invalid output id");
  }

  void Print() const override;
};

class RetVoidInst : public Instruction {
public:
  RetVoidInst() : Instruction(RetVoid) {}

  TERMINATOR_INST(0, 0);

  NO_INOUT();

  bool HasSideEffects() const override { return true; }

public:
  void Print() const override;
};

class CallInst : public Instruction {
public:
  CallInst(const char* Callee, const Operand& RetVal, const std::vector<Operand>& Args) : Instruction(Call), Callee_(Callee) {
    AddOperand(RetVal);
    for(auto& Arg : Args) {
      AddOperand(Arg);
    }
  }

  bool HasSideEffects() const override { return true; }

  virtual bool IsTerminator() const override { return false; }
  virtual size_t NumSuccessor() const override { return 0; }
  virtual BasicBlock* Successor(size_t Id) const override { 
    assert(false && "Invalid successor id");
    return nullptr;
  }
  virtual bool Verify() const override { return Size() >= 1; }

  virtual size_t Ins() const override { return Size() - 1; }
  virtual size_t Outs() const override { return 1; }

  virtual Operand GetIn(size_t Id) const override { 
    assert(Id < Ins() && "Invalid input id");
    return GetOperand(Id + 1); 
  }
  virtual Operand GetOut(size_t Id) const override { 
    assert(Id == 0 && "Invalid output id");
    return GetOperand(0); 
  }

  virtual void ReplaceIn(size_t Id, const Operand& New) override { 
    assert(Id < Ins() && "Invalid input id");
    SetOperand(Id + 1, New);
  }
  virtual void ReplaceOut(size_t Id, const Operand& New) override { 
    assert(Id == 0 && "Invalid output id");
    SetOperand(0, New);
  }

  void Print() const override;

  const char* Callee() const { return Callee_.c_str(); }

private:
  std::string Callee_;
};

class CallVoidInst : public Instruction {
public:
  CallVoidInst(const char* Callee, const std::vector<Operand>& Args) : Instruction(CallVoid), Callee_(Callee) {
    for(auto& Arg : Args) {
      AddOperand(Arg);
    }
  }

  bool HasSideEffects() const override { return true; }

  virtual bool IsTerminator() const override { return false; }
  virtual size_t NumSuccessor() const override { return 0; }
  virtual BasicBlock* Successor(size_t Id) const override { 
    assert(false && "Invalid successor id");
    return nullptr;
  }
  virtual bool Verify() const override { return Size() >= 0; }

  virtual size_t Ins() const override { return Size(); }
  virtual size_t Outs() const override { return 0; }

  virtual Operand GetIn(size_t Id) const override { 
    assert(Id < Ins() && "Invalid input id");
    return GetOperand(Id); 
  }
  virtual Operand GetOut(size_t Id) const override { 
    assert(false && "Invalid output id");
    return Operand();
  }

  virtual void ReplaceIn(size_t Id, const Operand& New) override { 
    assert(Id < Ins() && "Invalid input id");
    SetOperand(Id, New);
  }
  virtual void ReplaceOut(size_t Id, const Operand& New) override { 
    assert(false && "Invalid output id");
  }

  void Print() const override;

  const char* Callee() const { return Callee_.c_str(); }

private:
  std::string Callee_;
};

class ArrayNewInst : public Instruction {
public:
  ArrayNewInst(const Operand& RetVal, const Operand& Size) : Instruction(ArrayNew) {
    AddOperand(RetVal);
    AddOperand(Size);
  }

  NORMAL_INST(2);

  bool HasSideEffects() const override { return true; }

public:
  virtual size_t Ins() const override { return 1; }
  virtual size_t Outs() const override { return 1; }

  virtual Operand GetIn(size_t Id) const override { 
    assert(Id == 0 && "Invalid input id");
    return GetOperand(1); 
  }
  virtual Operand GetOut(size_t Id) const override { 
    assert(Id == 0 && "Invalid output id");
    return GetOperand(0); 
  }

  virtual void ReplaceIn(size_t Id, const Operand& New) override { 
    assert(Id == 0 && "Invalid input id");
    SetOperand(1, New);
  }
  virtual void ReplaceOut(size_t Id, const Operand& New) override { 
    assert(Id == 0 && "Invalid output id");
    SetOperand(0, New);
  }

  void Print() const override;
};

class ArrayLoadInst : public Instruction {
public:
  ArrayLoadInst(const Operand& RetVal, const Operand& Array, const Operand& Index) : Instruction(ArrayLoad) {
    AddOperand(RetVal);
    AddOperand(Array);
    AddOperand(Index);
  }

  NORMAL_INST(3);

  bool HasSideEffects() const override { return false; }

public:
  virtual size_t Ins() const override { return 2; }
  virtual size_t Outs() const override { return 1; }

  virtual Operand GetIn(size_t Id) const override { 
    assert(Id < 2 && "Invalid input id");
    return GetOperand(Id + 1); 
  }
  virtual Operand GetOut(size_t Id) const override { 
    assert(Id == 0 && "Invalid output id");
    return GetOperand(0); 
  }

  virtual void ReplaceIn(size_t Id, const Operand& New) override { 
    assert(Id < 2 && "Invalid input id");
    SetOperand(Id + 1, New);
  }
  virtual void ReplaceOut(size_t Id, const Operand& New) override { 
    assert(Id == 0 && "Invalid output id");
    SetOperand(0, New);
  }

  void Print() const override;
};

class ArrayStoreInst : public Instruction {
public:
  ArrayStoreInst(const Operand& Array, const Operand& Index, const Operand& Value) : Instruction(ArrayStore) {
    AddOperand(Array);
    AddOperand(Index);
    AddOperand(Value);
  }

  NORMAL_INST(3);

public:
  bool HasSideEffects() const override { return true; }

  virtual size_t Ins() const override { return 3; }
  virtual size_t Outs() const override { return 0; }

  virtual Operand GetIn(size_t Id) const override { 
    assert(Id < 3 && "Invalid input id");
    return GetOperand(Id); 
  }
  virtual Operand GetOut(size_t Id) const override { 
    assert(false && "Invalid output id");
    return Operand();
  }

  virtual void ReplaceIn(size_t Id, const Operand& New) override { 
    assert(Id < 3 && "Invalid input id");
    SetOperand(Id, New);
  }
  virtual void ReplaceOut(size_t Id, const Operand& New) override { 
    assert(false && "Invalid output id");
  }

  void Print() const override;
};

class LoadLabelInst : public Instruction {
public:
  LoadLabelInst(const Operand& Dst, const char* Label) : Instruction(LoadLabel), Label_(Label) {
    AddOperand(Dst);
  }

  NORMAL_INST(1);

  bool HasSideEffects() const override { return false; }

public:
  virtual size_t Ins() const override { return 0; }
  virtual size_t Outs() const override { return 1; }

  virtual Operand GetIn(size_t Id) const override { 
    assert(false && "Invalid input id");
    return Operand();
  }
  virtual Operand GetOut(size_t Id) const override { 
    assert(Id == 0 && "Invalid output id");
    return GetOperand(0); 
  }

  virtual void ReplaceIn(size_t Id, const Operand& New) override { 
    assert(false && "Invalid input id");
  }
  virtual void ReplaceOut(size_t Id, const Operand& New) override { 
    assert(Id == 0 && "Invalid output id");
    SetOperand(0, New);
  }

  void Print() const override;

  const char* Label() const { return Label_.c_str(); }

private:
  std::string Label_;
};

#undef NORMAL_INST
#undef TERMINATOR_INST
#undef NO_INOUT
#pragma endregion

class FuncBuilder {
public:
  FuncBuilder(const char* Name, size_t NumParams) : Function_(new Function(Name, NumParams)), CurrentBlock_(nullptr) {}

  BasicBlock* CreateBlock();
  void SetInsertionPoint(BasicBlock* BB) { CurrentBlock_ = BB; }
  BasicBlock* Current() const { return CurrentBlock_; }
  void NewBlock() { SetInsertionPoint(CreateBlock()); }

  Operand Imm(int64_t Value) { return Operand::CreateImmediate(Value); }
  Operand Param(size_t Id) { 
    assert(Id < Function_->NumParams() && "Invalid parameter id");
    return Operand::CreateParameter(Id); 
  }
  Operand NewReg() { return Operand::CreateRegister(Function_->NewReg()); }

    void Emit(Instruction* Inst);

  void Nop() { Emit(new NopInst()); }
  void Assign(const Operand& LHS, const Operand& RHS) { Emit(new AssignInst(LHS, RHS)); }

  void Add(const Operand& LHS, const Operand& RHS1, const Operand& RHS2) { Emit(new BinaryInst(BinaryInst::Add, LHS, RHS1, RHS2)); }
  void Sub(const Operand& LHS, const Operand& RHS1, const Operand& RHS2) { Emit(new BinaryInst(BinaryInst::Sub, LHS, RHS1, RHS2)); }
  void Mul(const Operand& LHS, const Operand& RHS1, const Operand& RHS2) { Emit(new BinaryInst(BinaryInst::Mul, LHS, RHS1, RHS2)); }
  void Div(const Operand& LHS, const Operand& RHS1, const Operand& RHS2) { Emit(new BinaryInst(BinaryInst::Div, LHS, RHS1, RHS2)); }
  void Mod(const Operand& LHS, const Operand& RHS1, const Operand& RHS2) { Emit(new BinaryInst(BinaryInst::Mod, LHS, RHS1, RHS2)); }
  void And(const Operand& LHS, const Operand& RHS1, const Operand& RHS2) { Emit(new BinaryInst(BinaryInst::And, LHS, RHS1, RHS2)); }
  void Or(const Operand& LHS, const Operand& RHS1, const Operand& RHS2) { Emit(new BinaryInst(BinaryInst::Or, LHS, RHS1, RHS2)); }
  void Xor(const Operand& LHS, const Operand& RHS1, const Operand& RHS2) { Emit(new BinaryInst(BinaryInst::Xor, LHS, RHS1, RHS2)); }
  void Shl(const Operand& LHS, const Operand& RHS1, const Operand& RHS2) { Emit(new BinaryInst(BinaryInst::Shl, LHS, RHS1, RHS2)); }
  void Shr(const Operand& LHS, const Operand& RHS1, const Operand& RHS2) { Emit(new BinaryInst(BinaryInst::Shr, LHS, RHS1, RHS2)); }
  void Lt(const Operand& LHS, const Operand& RHS1, const Operand& RHS2) { Emit(new BinaryInst(BinaryInst::Lt, LHS, RHS1, RHS2)); }
  void Le(const Operand& LHS, const Operand& RHS1, const Operand& RHS2) { Emit(new BinaryInst(BinaryInst::Le, LHS, RHS1, RHS2)); }
  void Gt(const Operand& LHS, const Operand& RHS1, const Operand& RHS2) { Emit(new BinaryInst(BinaryInst::Gt, LHS, RHS1, RHS2)); }
  void Ge(const Operand& LHS, const Operand& RHS1, const Operand& RHS2) { Emit(new BinaryInst(BinaryInst::Ge, LHS, RHS1, RHS2)); }
  void Eq(const Operand& LHS, const Operand& RHS1, const Operand& RHS2) { Emit(new BinaryInst(BinaryInst::Eq, LHS, RHS1, RHS2)); }
  void Ne(const Operand& LHS, const Operand& RHS1, const Operand& RHS2) { Emit(new BinaryInst(BinaryInst::Ne, LHS, RHS1, RHS2)); }

  void Jmp(BasicBlock* Target) { Emit(new JmpInst(Target)); }
  void Jnz(const Operand& Cond, BasicBlock* True, BasicBlock* False) { Emit(new JnzInst(Cond, True, False)); }
  void Ret(const Operand& RetVal) { Emit(new RetInst(RetVal)); }
  void RetVoid() { Emit(new RetVoidInst()); }

  void Call(const char* Callee, const Operand& RetVal, const std::vector<Operand>& Args) { Emit(new CallInst(Callee, RetVal, Args)); }
  void CallVoid(const char* Callee, const std::vector<Operand>& Args) { Emit(new CallVoidInst(Callee, Args)); }

  void ArrayNew(const Operand& RetVal, const Operand& Size) { Emit(new ArrayNewInst(RetVal, Size)); }
  void ArrayLoad(const Operand& RetVal, const Operand& Array, const Operand& Index) { Emit(new ArrayLoadInst(RetVal, Array, Index)); }
  void ArrayStore(const Operand& Array, const Operand& Index, const Operand& Value) { Emit(new ArrayStoreInst(Array, Index, Value)); }

  void LoadLabel(const Operand& Dst, const char* Label) { Emit(new LoadLabelInst(Dst, Label)); }

  Function* GetFunction() const { return Function_; }

private:
  Function* Function_;
  BasicBlock* CurrentBlock_;
};

} // namespace klang

#endif 