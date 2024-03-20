#ifndef _CODEGEN_H
#define _CODEGEN_H

#include <IR/IR.h>
#include <Semantic/IRGen.h>

#include <sstream>

namespace klang {

class MachineFunction;
class MachineBasicBlock;
class MachineInstruction;
class MachineOperand;

class MachineFunction {
public:
  MachineFunction(const std::string& Name, size_t NumParams)
    : Name_(Name), NumParams_(NumParams), Size_(0) {}
  ~MachineFunction();

  void AddBasicBlock(MachineBasicBlock* BB);

  void Emit(std::stringstream& SS) const;

  MachineBasicBlock* Entry() const { return BasicBlocks_.front(); }
  const char* Name() const { return Name_.c_str(); }

  std::vector<MachineBasicBlock*>::iterator begin() { return BasicBlocks_.begin(); }
  std::vector<MachineBasicBlock*>::iterator end() { return BasicBlocks_.end(); }
  std::vector<MachineBasicBlock*>::const_iterator begin() const { return BasicBlocks_.begin(); }
  std::vector<MachineBasicBlock*>::const_iterator end() const { return BasicBlocks_.end(); }

  std::vector<MachineBasicBlock*>::reverse_iterator rbegin() { return BasicBlocks_.rbegin(); }
  std::vector<MachineBasicBlock*>::reverse_iterator rend() { return BasicBlocks_.rend(); }
  std::vector<MachineBasicBlock*>::const_reverse_iterator rbegin() const { return BasicBlocks_.rbegin(); }
  std::vector<MachineBasicBlock*>::const_reverse_iterator rend() const { return BasicBlocks_.rend(); }

  std::vector<MachineBasicBlock*> PostOrder() const;

private:
  std::string Name_;
  size_t Size_, NumParams_;
  std::vector<MachineBasicBlock*> BasicBlocks_;
};

class MachineBasicBlock {
public:
  MachineBasicBlock(const char* Name) : Size_(0), Name_(Name), Head_(nullptr), Tail_(nullptr) {}
  ~MachineBasicBlock();

  const char* Name() const { return Name_.c_str(); }

  std::vector<MachineBasicBlock*> Successors() const;
  std::vector<MachineBasicBlock*> Predecessors() const;

  void AddInstruction(MachineInstruction* Inst);

  void Emit(std::stringstream& Out) const;

  bool IsExit() const;

  class iterator {
  public:
    iterator(MachineInstruction* Inst, bool Direction) : Inst_(Inst), Direction_(Direction) {
      UpdateLinks();
    }
    MachineInstruction* operator*() { return Inst_; }
    iterator& operator++();
    iterator operator++(int);

    bool operator!=(const iterator& Other) const { return Inst_ != Other.Inst_; }
  
  private:
    void UpdateLinks();

    MachineInstruction* Inst_, *Next_, *Prev_;
    bool Direction_;
  };

  iterator begin() { return iterator(Head_, true); }
  iterator end() { return iterator(nullptr, true); }

  iterator rbegin() { return iterator(Tail_, false); }
  iterator rend() { return iterator(nullptr, false); }

  void InsertBefore(MachineInstruction* Inst, MachineInstruction* Before);
  void InsertAfter(MachineInstruction* Inst, MachineInstruction* After);
  MachineInstruction* Replace(MachineInstruction* Inst, MachineInstruction* Target);
  MachineInstruction* Remove(MachineInstruction* Inst);

protected:
  friend class MachineFunction;

  void SetParent(MachineFunction* Parent) { Parent_ = Parent; }

private:
  size_t Size_;
  MachineFunction* Parent_;
  std::string Name_;
  MachineInstruction* Head_, *Tail_;
};

class MachineInstruction {
public:
  enum class Opcode : int {
    Mov, 
    CMov, 

    Add,
    Sub,
    IMul,
    IDiv,
    Or,
    Xor,
    And,
    Shl,
    Shr,

    Test,
    Cmp,

    Jmp,
    Jcc, 
    Ret,

    Push,
    Pop, 

    Call, 
    Lea,
    Cqo, 
  };

  MachineInstruction(Opcode Opcode) : Opcode_(Opcode), Next_(nullptr), Prev_(nullptr), Operands_(), Parent_(nullptr) {}
  virtual ~MachineInstruction();

  void AddOperand(const MachineOperand& Op);
  MachineOperand GetOperand(size_t Idx) const;
  void ReplaceOperand(size_t Idx, const MachineOperand& Op);
  size_t Size() const { return Operands_.size(); }

  virtual bool Verify() const = 0;
  virtual bool HasSideEffects() const = 0;
  virtual bool IsTerminator() const = 0;
  virtual void Emit(std::stringstream& Out) const = 0;

  virtual size_t NumSuccessors() const = 0;
  virtual MachineBasicBlock* GetSuccessor(size_t Idx) const = 0;

  MachineBasicBlock* Parent() const { return Parent_; } 
  Opcode GetOpcode() const { return Opcode_; }

protected:
  friend class MachineBasicBlock;

  void SetNext(MachineInstruction* Next) { Next_ = Next; }
  void SetPrev(MachineInstruction* Prev) { Prev_ = Prev; }
  MachineInstruction* Next() const { return Next_; }
  MachineInstruction* Prev() const { return Prev_; }
  void SetParent(MachineBasicBlock* Parent) { Parent_ = Parent; }

private:
  Opcode Opcode_;
  MachineBasicBlock* Parent_;
  MachineInstruction* Next_, *Prev_;
  std::vector<MachineOperand> Operands_;
};

enum MachineRegister : int {
  None,   // For memory operands

  // retval & temp
  RAX,

  // allocatables 
  RCX,
  R8,
  R9,
  R10,
  R11,
  RDX,
  RSI,
  RDI,

  // callee-saved
  RBX,
  RBP,
  RSP,
  R12,
  R13,
  R14,
  R15
};

const char* GetRegisterName(MachineRegister Reg);

class MachineOperand {
public:
  enum class Kind : int {
    VirtualRegister,
    Register,
    Immediate,
    Memory
  };

  static constexpr size_t WordSize() { return 8; }

  static MachineOperand CreateRegister(MachineRegister Reg) {
    MachineOperand Op;
    Op.Kind_ = Kind::Register;
    Op.U_.Reg_ = Reg;
    return Op;
  }

  static MachineOperand CreateVirtualRegister(size_t Id) {
    MachineOperand Op;
    Op.Kind_ = Kind::VirtualRegister;
    Op.U_.RegId_ = Id;
    return Op;
  }

  static MachineOperand CreateImmediate(int64_t Imm) {
    MachineOperand Op;
    Op.Kind_ = Kind::Immediate;
    Op.U_.Imm_ = Imm;
    return Op;
  }

  static MachineOperand CreateMemory(MachineRegister Base) {
    MachineOperand Op;
    Op.Kind_ = Kind::Memory;
    Op.U_.Memory.Base_ = Base;
    Op.U_.Memory.Index_ = None;
    Op.U_.Memory.Disp_ = 0;
    return Op;
  }

  static MachineOperand CreateMemory(MachineRegister Base, int64_t Disp) {
    MachineOperand Op;
    Op.Kind_ = Kind::Memory;
    Op.U_.Memory.Base_ = Base;
    Op.U_.Memory.Index_ = None;
    Op.U_.Memory.Disp_ = Disp;
    return Op;
  }

  static MachineOperand CreateMemory(MachineRegister Base, MachineRegister Index, int64_t Disp) {
    MachineOperand Op;
    Op.Kind_ = Kind::Memory;
    Op.U_.Memory.Base_ = Base;
    Op.U_.Memory.Index_ = Index;
    Op.U_.Memory.Disp_ = Disp;
    return Op;
  }

  bool IsRegister() const { return Kind_ == Kind::Register || Kind_ == Kind::VirtualRegister; }
  bool IsMachineRegister() const { return Kind_ == Kind::Register; }
  bool IsVirtualRegister() const { return Kind_ == Kind::VirtualRegister; }
  bool IsMemory() const { return Kind_ == Kind::Memory; }
  bool IsImmediate() const { return Kind_ == Kind::Immediate; }
  
  bool IsRM() const { return IsRegister() || IsMemory(); }

  size_t GetVirtualRegister() const { assert(IsVirtualRegister() && "Invalid operand kind"); return U_.RegId_; }
  MachineRegister GetRegister() const { assert(IsRegister() && "Invalid operand kind"); return U_.Reg_; }
  int64_t GetImmediate() const { assert(IsImmediate() && "Invalid operand kind"); return U_.Imm_; }

  void Emit(std::stringstream& Out) const;

private:
  void EmitRegister(std::stringstream& Out, MachineRegister Reg) const;
  void EmitImmediate(std::stringstream& Out, int64_t Imm) const;
  void EmitMemory(std::stringstream& Out, MachineRegister Base, MachineRegister Index, int64_t Disp_) const;

  Kind Kind_;
  union {
    size_t RegId_;
    MachineRegister Reg_;
    int64_t Imm_;
    struct {
      MachineRegister Base_;
      MachineRegister Index_;
      int64_t Disp_;
    } Memory;
  } U_;
};

/// Instructions
#define NO_SUCCESSORS() \
  virtual bool IsTerminator() const override { return false; } \
  size_t NumSuccessors() const override { return 0; } \
  MachineBasicBlock* GetSuccessor(size_t Idx) const override { return nullptr; }

enum class Condition : int {
  E, NE, L, LE, G, GE
};

class MovMachineInst : public MachineInstruction {
public:
  MovMachineInst(const MachineOperand& Src, const MachineOperand& Dst) : MachineInstruction(Opcode::Mov) {
    AddOperand(Src);
    AddOperand(Dst);
  }

  virtual bool Verify() const override;
  virtual bool HasSideEffects() const override { return false; }
  virtual void Emit(std::stringstream& Out) const override;

  NO_SUCCESSORS();
  
};

class CMovMachineInst : public MachineInstruction {
public:
  CMovMachineInst(const MachineOperand& Src, const MachineOperand& Dst, Condition Cond) : MachineInstruction(Opcode::CMov), Cond_(Cond) {
    AddOperand(Src);
    AddOperand(Dst);
  }

  virtual bool Verify() const override;
  virtual bool HasSideEffects() const override { return false; }
  virtual void Emit(std::stringstream& Out) const override;

  Condition GetCondition() const { return Cond_; }

  NO_SUCCESSORS();

private:
  Condition Cond_;
};

class AddMachineInst : public MachineInstruction {
public:
  AddMachineInst(const MachineOperand& Src, const MachineOperand& Dst) : MachineInstruction(Opcode::Add) {
    AddOperand(Src);
    AddOperand(Dst);
  }

  virtual bool Verify() const override;
  virtual bool HasSideEffects() const override { return false; }
  virtual void Emit(std::stringstream& Out) const override;

  NO_SUCCESSORS();
};

class SubMachineInst : public MachineInstruction {
public:
  SubMachineInst(const MachineOperand& Src, const MachineOperand& Dst) : MachineInstruction(Opcode::Sub) {
    AddOperand(Src);
    AddOperand(Dst);
  }

  virtual bool Verify() const override;
  virtual bool HasSideEffects() const override { return false; }
  virtual void Emit(std::stringstream& Out) const override;

  NO_SUCCESSORS();
};

class IMulMachineInst : public MachineInstruction {
public:
  IMulMachineInst(const MachineOperand& Src, const MachineOperand& Dst) : MachineInstruction(Opcode::IMul) {
    AddOperand(Src);
    AddOperand(Dst);
  }

  virtual bool Verify() const override;
  virtual bool HasSideEffects() const override { return true; }
  virtual void Emit(std::stringstream& Out) const override;

  NO_SUCCESSORS();
};

class IDivMachineInst : public MachineInstruction {
public:
  IDivMachineInst(const MachineOperand& Src) : MachineInstruction(Opcode::IDiv) {
    AddOperand(Src);
  }

  virtual bool Verify() const override;
  virtual bool HasSideEffects() const override { return true; }
  virtual void Emit(std::stringstream& Out) const override;

  NO_SUCCESSORS();
};

class OrMachineInst : public MachineInstruction {
public:
  OrMachineInst(const MachineOperand& Src, const MachineOperand& Dst) : MachineInstruction(Opcode::Or) {
    AddOperand(Src);
    AddOperand(Dst);
  }

  virtual bool Verify() const override;
  virtual bool HasSideEffects() const override { return false; }
  virtual void Emit(std::stringstream& Out) const override;

  NO_SUCCESSORS();
};

class XorMachineInst : public MachineInstruction {
public:
  XorMachineInst(const MachineOperand& Src, const MachineOperand& Dst) : MachineInstruction(Opcode::Xor) {
    AddOperand(Src);
    AddOperand(Dst);
  }

  virtual bool Verify() const override;
  virtual bool HasSideEffects() const override { return false; }
  virtual void Emit(std::stringstream& Out) const override;

  NO_SUCCESSORS();
};

class AndMachineInst : public MachineInstruction {
public:
  AndMachineInst(const MachineOperand& Src, const MachineOperand& Dst) : MachineInstruction(Opcode::And) {
    AddOperand(Src);
    AddOperand(Dst);
  }

  virtual bool Verify() const override;
  virtual bool HasSideEffects() const override { return false; }
  virtual void Emit(std::stringstream& Out) const override;

  NO_SUCCESSORS();
};

class TestMachineInst : public MachineInstruction {
public:
  TestMachineInst(const MachineOperand& Op1, const MachineOperand& Op2) : MachineInstruction(Opcode::Test) {
    AddOperand(Op1);
    AddOperand(Op2);
  }

  virtual bool Verify() const override;
  virtual bool HasSideEffects() const override { return false; }
  virtual void Emit(std::stringstream& Out) const override;

  NO_SUCCESSORS();
};

class CmpMachineInst : public MachineInstruction {
public:
  CmpMachineInst(const MachineOperand& Op1, const MachineOperand& Op2) : MachineInstruction(Opcode::Cmp) {
    AddOperand(Op1);
    AddOperand(Op2);
  }

  virtual bool Verify() const override { return true; }
  virtual bool HasSideEffects() const override { return false; }
  virtual void Emit(std::stringstream& Out) const override;

  NO_SUCCESSORS();
};

class JmpMachineInst : public MachineInstruction {
public:
  JmpMachineInst(MachineBasicBlock* Target) : MachineInstruction(Opcode::Jmp), Target_(Target) {}

  virtual bool Verify() const override { return true; }
  virtual bool HasSideEffects() const override { return true; }
  virtual void Emit(std::stringstream& Out) const override;

  virtual bool IsTerminator() const override { return true; }
  virtual size_t NumSuccessors() const override { return 1; }
  virtual MachineBasicBlock* GetSuccessor(size_t Idx) const override { 
    assert(Idx == 0 && "Invalid successor index");
    return Target_; 
  }

private:
  MachineBasicBlock* Target_;
};

class JccMachineInst : public MachineInstruction {
public:
  JccMachineInst(Condition Cond, MachineBasicBlock* True, MachineBasicBlock* False) : MachineInstruction(Opcode::Jcc), Cond_(Cond), True_(True), False_(False) {}

  virtual bool IsTerminator() const override { return true; }
  virtual bool Verify() const override { return true; }
  virtual bool HasSideEffects() const override { return true; }
  virtual void Emit(std::stringstream& Out) const override;

  virtual size_t NumSuccessors() const override { return 2; }
  virtual MachineBasicBlock* GetSuccessor(size_t Idx) const override { 
    assert(Idx < 2 && "Invalid successor index");
    return Idx == 1 ? True_ : False_;
  }

private:
  Condition Cond_;
  MachineBasicBlock* True_, *False_;
};

class RetMachineInst : public MachineInstruction {
public:
  RetMachineInst() : MachineInstruction(Opcode::Ret) {}

  virtual bool Verify() const override { return true; }
  virtual bool HasSideEffects() const override { return true; }
  virtual void Emit(std::stringstream& Out) const override;

  virtual bool IsTerminator() const override { return true; }
  virtual size_t NumSuccessors() const override { return 0; }
  virtual MachineBasicBlock* GetSuccessor(size_t Idx) const override { 
    assert(false && "Invalid successor index");
    return nullptr; 
  }
};

class PushMachineInst : public MachineInstruction {
public:
  PushMachineInst(const MachineOperand& Op) : MachineInstruction(Opcode::Push) {
    AddOperand(Op);
  }

  virtual bool Verify() const override { 
    return GetOperand(0).IsMachineRegister() || GetOperand(0).IsImmediate();
  }
  virtual bool HasSideEffects() const override { return false; }
  virtual void Emit(std::stringstream& Out) const override;

  NO_SUCCESSORS();
};

class PopMachineInst : public MachineInstruction {
public:
  PopMachineInst(const MachineOperand& Op) : MachineInstruction(Opcode::Pop) {
    AddOperand(Op);
  }

  virtual bool Verify() const override { 
    return GetOperand(0).IsMachineRegister();
  }
  virtual bool HasSideEffects() const override { return false; }
  virtual void Emit(std::stringstream& Out) const override;

  NO_SUCCESSORS();
};

class CallMachineInst : public MachineInstruction {
public:
  CallMachineInst(const char* Callee) : MachineInstruction(Opcode::Call), Callee_(Callee) {}

  virtual bool Verify() const override { return true; }
  virtual bool HasSideEffects() const override { return true; }

  virtual void Emit(std::stringstream& Out) const override;

  NO_SUCCESSORS();

private:
  std::string Callee_;
};

class LeaMachineInst : public MachineInstruction {
public:
  LeaMachineInst(const char* Label, const MachineOperand& Dst) : MachineInstruction(Opcode::Lea), Label_(Label) {
    AddOperand(Dst);
  }

  virtual bool Verify() const override { return true; }
  virtual bool HasSideEffects() const override { return false; }
  virtual void Emit(std::stringstream& Out) const override;

  const char* Label() const { return Label_.c_str(); }

  NO_SUCCESSORS();

private:
  std::string Label_;
};

class CqoMachineInst : public MachineInstruction {
public:
  CqoMachineInst() : MachineInstruction(Opcode::Cqo) {}

  virtual bool Verify() const override { return true; }
  virtual bool HasSideEffects() const override { return false; }
  virtual void Emit(std::stringstream& Out) const override;

  NO_SUCCESSORS();
};

#undef NO_SUCCESSORS

class MachineFuncBuilder {
public:
  MachineFuncBuilder(Function* Function) : Function_(Function), MFunction_(nullptr), CurrentBlock_(nullptr), NumRegs_(0) {
    MFunction_ = new MachineFunction(Function->Name(), Function->NumParams());
    CurrentBlock_ = nullptr;
  }

  MachineFunction* GetFunction() { return MFunction_; }
  void Generate();

  MachineBasicBlock* CreateBlock(const char* Name);
  void SetInsertionPoint(MachineBasicBlock* BB) { CurrentBlock_ = BB; }

  void Emit(MachineInstruction* Inst);

private:
  void GenerateBasicBlock(BasicBlock* BB);
  void GenerateInstruction(Instruction& Inst);

  void HandleBinaryInst(BinaryInst& Inst);
  void HandleLogicalBinaryInst(BinaryInst& Inst);

  MachineOperand ConvertOperand(const Operand& Op);
  MachineOperand NewReg() {
    return MachineOperand::CreateVirtualRegister(NumRegs_++);
  }

  void Mov(const MachineOperand& Src, const MachineOperand& Dst) {
    Emit(new MovMachineInst(Src, Dst));
  }
  void CMov(const MachineOperand& Src, const MachineOperand& Dst, Condition Cond) {
    Emit(new CMovMachineInst(Src, Dst, Cond));
  }
  void Add(const MachineOperand& Src, const MachineOperand& Dst) {
    Emit(new AddMachineInst(Src, Dst));
  }
  void Sub(const MachineOperand& Src, const MachineOperand& Dst) {
    Emit(new SubMachineInst(Src, Dst));
  }
  void IMul(const MachineOperand& Src, const MachineOperand& Dst) {
    Emit(new IMulMachineInst(Src, Dst));
  }
  void IDiv(const MachineOperand& Src) {
    Emit(new IDivMachineInst(Src));
  }
  void And(const MachineOperand& Src, const MachineOperand& Dst) {
    Emit(new AndMachineInst(Src, Dst));
  }
  void Or(const MachineOperand& Src, const MachineOperand& Dst) {
    Emit(new OrMachineInst(Src, Dst));
  }
  void Xor(const MachineOperand& Src, const MachineOperand& Dst) {
    Emit(new XorMachineInst(Src, Dst));
  }
  void Test(const MachineOperand& Op1, const MachineOperand& Op2) {
    Emit(new TestMachineInst(Op1, Op2));
  }
  void Cmp(const MachineOperand& Op1, const MachineOperand& Op2) {
    Emit(new CmpMachineInst(Op1, Op2));
  }

  void Push(const MachineOperand& Op) {
    Emit(new PushMachineInst(Op));
  }
  void Pop(const MachineOperand& Op) {
    Emit(new PopMachineInst(Op));
  }

  void Jmp(BasicBlock* Target) {
    auto *MBB = BBMap_[Target];
    Emit(new JmpMachineInst(MBB));
  }
  void Jcc(BasicBlock* True, BasicBlock* False, Condition Cond) {
    auto *MBBTrue = BBMap_[True];
    auto *MBBFalse = BBMap_[False];
    Emit(new JccMachineInst(Cond, MBBTrue, MBBFalse));
  }
  void Ret() {
    Emit(new RetMachineInst());
  }
  void Call(const char* Callee) {
    Emit(new CallMachineInst(Callee));
  }
  void Lea(const char* Label, const MachineOperand& Dst) {
    Emit(new LeaMachineInst(Label, Dst));
  }
  void Cqo() {
    Emit(new CqoMachineInst());
  }

  Function* Function_;
  MachineFunction* MFunction_;
  MachineBasicBlock* CurrentBlock_;
  std::unordered_map<BasicBlock*, MachineBasicBlock*> BBMap_;
  size_t NumRegs_;
  std::unordered_map<size_t, size_t> VirtRegMap_;
};

class ModuleCodegen {
public:
  ModuleCodegen(Module* Module, ModuleGenCtx* IRGenCtx) : Module_(Module), IRGenCtx_(IRGenCtx) {}

  bool Generate();

  bool Save(const char* OutputName);

private:
  void GenerateStringLiterals();

  Module* Module_; 
  ModuleGenCtx* IRGenCtx_;
  std::stringstream ModuleSS_;
};

} // namespace klang

#endif 