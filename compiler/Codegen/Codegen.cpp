#include <Codegen/Codegen.h> 
#include <Codegen/RegAlloc.h>
#include <Codegen/InstSched.h>
#include <Logging.h>

#include <exception>

const char* kFunctionPrefix = "K_";

namespace klang {

MachineFunction::~MachineFunction() {
  for(auto BB : BasicBlocks_) {
    delete BB;
  }
}

MachineBasicBlock::~MachineBasicBlock() {
  auto *Inst = Head_;
  while(Inst != nullptr) {
    auto *Next = Inst->Next();
    delete Inst;
    Inst = Next;
  }
}

MachineInstruction::~MachineInstruction() {
}

void MachineFunction::AddBasicBlock(MachineBasicBlock* BB) {
  BasicBlocks_.push_back(BB);
  BB->SetParent(this);
  Size_++;
}

void MachineFunction::Emit(std::stringstream& SS) const {
  SS << ".global " << kFunctionPrefix << Name() << '\n';
  SS << kFunctionPrefix << Name() << ":\n";
  for(auto *BB : BasicBlocks_) {
    BB->Emit(SS);
  }
}

std::vector<MachineBasicBlock*> MachineFunction::PostOrder() const {
  std::vector<MachineBasicBlock*> Result;
  std::set<MachineBasicBlock*> Visited;

  assert(BasicBlocks_.size() > 0 && "Empty function");

  std::function<void(MachineBasicBlock*)> Visitor = [&](MachineBasicBlock* BB) {
    if(Visited.count(BB) > 0) {
      return;
    }
    Visited.insert(BB);
    for(auto *Succ : BB->Successors()) {
      Visitor(Succ);
    }
    Result.push_back(BB);
  };
  Visitor(BasicBlocks_[0]);
  return Result;
}

void MachineBasicBlock::AddInstruction(MachineInstruction* Inst) {
  if(Head_ == nullptr) {
    Head_ = Inst;
    Tail_ = Inst;
  } else {
    Tail_->SetNext(Inst);
    Inst->SetPrev(Tail_);
    Tail_ = Inst;
  }
  Inst->SetParent(this);
  Size_++;
}

std::vector<MachineBasicBlock*> MachineBasicBlock::Successors() const {
  std::vector<MachineBasicBlock*> Succs;
  auto *Inst = Tail_;
  assert(Inst != nullptr && "Empty basic block");
  assert(Inst->IsTerminator() && "Basic block does not end with a terminator");

  for(size_t i = 0; i < Inst->NumSuccessors(); ++i) {
    Succs.push_back(Inst->GetSuccessor(i));
  }
  return Succs;
}

std::vector<MachineBasicBlock*> MachineBasicBlock::Predecessors() const {
  std::vector<MachineBasicBlock*> Preds;
  for(auto *BB : (*Parent_)) {
    for(auto *Succ : BB->Successors()) {
      if(Succ == this) {
        Preds.push_back(BB);
      }
    }
  }
  return Preds;
}

void MachineBasicBlock::Emit(std::stringstream& SS) const {
  SS << Name() << ":\n";
  for(auto *Inst = Head_; Inst != nullptr; Inst = Inst->Next()) {
    Inst->Emit(SS);
    SS << '\n';
  }
}

void MachineBasicBlock::iterator::UpdateLinks() {
  if(Inst_ == nullptr) {
    Next_ = nullptr;
    Prev_ = nullptr;
    return;
  }
  Next_ = Inst_->Next();
  Prev_ = Inst_->Prev();
}

MachineBasicBlock::iterator& MachineBasicBlock::iterator::operator++() {
  Inst_ = Direction_ ? Next_ : Prev_;
  UpdateLinks();
  return *this;
}

MachineBasicBlock::iterator MachineBasicBlock::iterator::operator++(int) {
  iterator Temp = *this;
  Inst_ = Direction_ ? Next_ : Prev_;
  UpdateLinks();
  return Temp;
}

bool MachineBasicBlock::IsExit() const {
  return Tail_ != nullptr && Tail_->GetOpcode() == MachineInstruction::Opcode::Ret;
}

void MachineBasicBlock::InsertBefore(MachineInstruction* Inst, MachineInstruction* Before) {
  assert(Before->Parent() == this && "Instruction does not belong to this basic block");
  assert(Inst->Parent() == nullptr && "Instruction already belongs to a basic block");

  if(Head_ == Before) {
    Inst->SetNext(Before);
    Before->SetPrev(Inst);
    Head_ = Inst;
  } else {
    auto *Prev = Before->Prev();
    Inst->SetPrev(Prev);
    Inst->SetNext(Before);
    Before->SetPrev(Inst);
    Prev->SetNext(Inst);
  }
  Inst->SetParent(this);
  Size_++;
}

void MachineBasicBlock::InsertAfter(MachineInstruction* Inst, MachineInstruction* After) {
  assert(After->Parent() == this && "Instruction does not belong to this basic block");
  assert(Inst->Parent() == nullptr && "Instruction already belongs to a basic block");

  if(Tail_ == After) {
    Inst->SetPrev(After);
    After->SetNext(Inst);
    Tail_ = Inst;
  } else {
    auto *Next = After->Next();
    Inst->SetPrev(After);
    Inst->SetNext(Next);
    After->SetNext(Inst);
    Next->SetPrev(Inst);
  }
  Inst->SetParent(this);
  Size_++;
}

MachineInstruction* MachineBasicBlock::Replace(MachineInstruction* Inst, MachineInstruction* Target) {
  assert(Target->Parent() == this && "Target instruction does not belong to this basic block");
  assert(Inst->Parent() == nullptr && "Instruction already belongs to a basic block");

  if(Head_ == Target) {
    Head_ = Inst;
  }
  if(Tail_ == Target) {
    Tail_ = Inst;
  }

  auto *Prev = Target->Prev();
  if(Prev != nullptr) {
    Prev->SetNext(Inst);
  }
  Inst->SetPrev(Prev);
  auto *Next = Target->Next();
  if(Next != nullptr) {
    Next->SetPrev(Inst);
  }
  Inst->SetNext(Next);
  Inst->SetParent(this);

  Target->SetNext(nullptr);
  Target->SetPrev(nullptr);
  Target->SetParent(nullptr);
  return Target;
}

MachineInstruction* MachineBasicBlock::Remove(MachineInstruction* Inst) {
  assert(Inst->Parent() == this && "Instruction does not belong to this basic block");

  if(Head_ == Inst) {
    Head_ = Inst->Next();
  }
  if(Tail_ == Inst) {
    Tail_ = Inst->Prev();
  }

  auto *Prev = Inst->Prev();
  auto *Next = Inst->Next();
  if(Prev != nullptr) {
    Prev->SetNext(Next);
  }
  if(Next != nullptr) {
    Next->SetPrev(Prev);
  }
  Inst->SetNext(nullptr);
  Inst->SetPrev(nullptr);
  Inst->SetParent(nullptr);
  Size_--;
  return Inst;
}

void MachineInstruction::AddOperand(const MachineOperand& Op) {
  Operands_.push_back(Op);
}

MachineOperand MachineInstruction::GetOperand(size_t Idx) const {
  assert(Idx < Operands_.size() && "Invalid operand index");
  return Operands_[Idx];
}

void MachineInstruction::ReplaceOperand(size_t Idx, const MachineOperand& Op) {
  assert(Idx < Operands_.size() && "Invalid operand index");
  Operands_[Idx] = Op;
}

void MachineOperand::Emit(std::stringstream& SS) const {
  if(IsMachineRegister()) {
    EmitRegister(SS, U_.Reg_);
  } else if(IsImmediate()) {
    EmitImmediate(SS, U_.Imm_);
  } else if(IsMemory()) {
    EmitMemory(SS, U_.Memory.Base_, U_.Memory.Index_, U_.Memory.Disp_);
  } else if(IsVirtualRegister()) {
    SS << "vreg" << U_.RegId_;
  }
}

const char* GetRegisterName(MachineRegister Reg) {
  switch(Reg) {
    case MachineRegister::None: return "none";
    case MachineRegister::RAX: return "rax";
    case MachineRegister::RBX: return "rbx";
    case MachineRegister::RCX: return "rcx";
    case MachineRegister::RDX: return "rdx";
    case MachineRegister::RSP: return "rsp";
    case MachineRegister::RBP: return "rbp";
    case MachineRegister::RSI: return "rsi";
    case MachineRegister::RDI: return "rdi";
    case MachineRegister::R8: return "r8";
    case MachineRegister::R9: return "r9";
    case MachineRegister::R10: return "r10";
    case MachineRegister::R11: return "r11";
    case MachineRegister::R12: return "r12";
    case MachineRegister::R13: return "r13";
    case MachineRegister::R14: return "r14";
    case MachineRegister::R15: return "r15";
    default: __builtin_unreachable();
  }
}

void MachineOperand::EmitRegister(std::stringstream& SS, MachineRegister Reg) const {
  SS << GetRegisterName(Reg);
}

void MachineOperand::EmitImmediate(std::stringstream& SS, int64_t Imm) const {
  SS << std::hex << "0x" << Imm << std::dec;
}

void MachineOperand::EmitMemory(std::stringstream& SS, MachineRegister Base, MachineRegister Index, int64_t Disp) const {
  SS << "qword ptr [";
  EmitRegister(SS, Base);
  if(Index != MachineRegister::None) {
    SS << " + ";
    EmitRegister(SS, Index);
  }
  if(Disp != 0) {
    if(Disp > 0) {
      SS << " + " << Disp;
    } else {
      SS << " - " << -Disp;
    }
  }
  SS << "]";
}

static bool RMICheck(const MachineOperand& Src, const MachineOperand& Dst) {
  if(Src.IsRegister() && Dst.IsRegister()) {
    return true;
  }
  if(Src.IsImmediate() && Dst.IsRegister()) {
    return true;
  }
  if(Src.IsRegister() && Dst.IsMemory()) {
    return true;
  }
  if(Src.IsMemory() && Dst.IsRegister()) {
    return true;
  }
  return false;
}

bool MovMachineInst::Verify() const {
  if(Size() != 2) {
    return false;
  }
  return RMICheck(GetOperand(0), GetOperand(1));
}

void MovMachineInst::Emit(std::stringstream& SS) const {
  SS << "mov ";
  GetOperand(1).Emit(SS);
  SS << ", ";
  GetOperand(0).Emit(SS);
}

bool CMovMachineInst::Verify() const {
  if(Size() != 3) {
    return false;
  }
  return GetOperand(1).IsRegister() && GetOperand(0).IsRM();
};

bool TestMachineInst::Verify() const {
  if(Size() != 2) {
    return false;
  }
  return RMICheck(GetOperand(0), GetOperand(1));
}

void TestMachineInst::Emit(std::stringstream& SS) const {
  SS << "test ";
  GetOperand(1).Emit(SS);
  SS << ", ";
  GetOperand(0).Emit(SS);
}

void CmpMachineInst::Emit(std::stringstream& SS) const {
  SS << "cmp ";
  GetOperand(1).Emit(SS);
  SS << ", ";
  GetOperand(0).Emit(SS);
}

void CMovMachineInst::Emit(std::stringstream& SS) const {
  SS << "cmov";
  switch(Cond_) {
    case Condition::E: SS << "e"; break;
    case Condition::NE: SS << "ne"; break;
    case Condition::L: SS << "l"; break;
    case Condition::LE: SS << "le"; break;
    case Condition::G: SS << "g"; break;
    case Condition::GE: SS << "ge"; break;
    default: __builtin_unreachable();
  }
  SS << " ";
  GetOperand(1).Emit(SS);
  SS << ", ";
  GetOperand(0).Emit(SS);
}

void JmpMachineInst::Emit(std::stringstream& SS) const {
  SS << "jmp " << Target_->Name();
}

void JccMachineInst::Emit(std::stringstream& SS) const {
  SS << "j";
  switch(Cond_) {
    case Condition::E: SS << "e"; break;
    case Condition::NE: SS << "ne"; break;
    case Condition::L: SS << "l"; break;
    case Condition::LE: SS << "le"; break;
    case Condition::G: SS << "g"; break;
    case Condition::GE: SS << "ge"; break;
    default: __builtin_unreachable();
  }
  SS << " " << True_->Name() << '\n';
  SS << "jmp " << False_->Name();
}

bool AddMachineInst::Verify() const {
  if(Size() != 2) {
    return false;
  }
  return RMICheck(GetOperand(0), GetOperand(1));
}

void AddMachineInst::Emit(std::stringstream& SS) const {
  SS << "add ";
  GetOperand(1).Emit(SS);
  SS << ", ";
  GetOperand(0).Emit(SS);
}

bool SubMachineInst::Verify() const {
  if(Size() != 2) {
    return false;
  }
  return RMICheck(GetOperand(0), GetOperand(1));
}

void SubMachineInst::Emit(std::stringstream& SS) const {
  SS << "sub ";
  GetOperand(1).Emit(SS);
  SS << ", ";
  GetOperand(0).Emit(SS);
}

bool IMulMachineInst::Verify() const {
  if(Size() != 2) {
    return false;
  }
  return GetOperand(1).IsRegister() && GetOperand(0).IsRM();  
}

void IMulMachineInst::Emit(std::stringstream& SS) const {
  SS << "imul ";
  GetOperand(1).Emit(SS);
  SS << ", ";
  GetOperand(0).Emit(SS);
}

bool IDivMachineInst::Verify() const {
  if(Size() != 1) {
    return false;
  }
  return GetOperand(0).IsRM();
}

void IDivMachineInst::Emit(std::stringstream& SS) const {
  SS << "idiv ";
  GetOperand(0).Emit(SS);
}

bool OrMachineInst::Verify() const {
  if(Size() != 2) {
    return false;
  }
  return RMICheck(GetOperand(0), GetOperand(1));
}

void OrMachineInst::Emit(std::stringstream& SS) const {
  SS << "or ";
  GetOperand(1).Emit(SS);
  SS << ", ";
  GetOperand(0).Emit(SS);
}

bool AndMachineInst::Verify() const {
  if(Size() != 2) {
    return false;
  }
  return RMICheck(GetOperand(0), GetOperand(1));
}

void AndMachineInst::Emit(std::stringstream& SS) const {
  SS << "and ";
  GetOperand(1).Emit(SS);
  SS << ", ";
  GetOperand(0).Emit(SS);
}

bool XorMachineInst::Verify() const {
  if(Size() != 2) {
    return false;
  }
  return RMICheck(GetOperand(0), GetOperand(1));
}

void XorMachineInst::Emit(std::stringstream& SS) const {
  SS << "xor ";
  GetOperand(1).Emit(SS);
  SS << ", ";
  GetOperand(0).Emit(SS);
}

void RetMachineInst::Emit(std::stringstream& SS) const {
  SS << "ret";
}

void PushMachineInst::Emit(std::stringstream& SS) const {
  SS << "push ";
  GetOperand(0).Emit(SS);
}

void PopMachineInst::Emit(std::stringstream& SS) const {
  SS << "pop ";
  GetOperand(0).Emit(SS);
}

void CallMachineInst::Emit(std::stringstream& SS) const {
  SS << "call " << kFunctionPrefix << Callee_;
}

void LeaMachineInst::Emit(std::stringstream& SS) const {
  SS << "lea ";
  GetOperand(0).Emit(SS);
  SS << ", " << Label_;
}

void CqoMachineInst::Emit(std::stringstream& SS) const {
  SS << "cqo";
}

MachineBasicBlock* MachineFuncBuilder::CreateBlock(const char* Name) {
  auto *BB = new MachineBasicBlock(Name);
  MFunction_->AddBasicBlock(BB);
  return BB;
}

void MachineFuncBuilder::Emit(MachineInstruction* Inst) {
  assert(CurrentBlock_ && "No current basic block");
  CurrentBlock_->AddInstruction(Inst);
}

void MachineFuncBuilder::Generate() {
  for(auto *BB : (*Function_)) {
    GenerateBasicBlock(BB);
  }
  for(auto *BB : (*Function_)) {
    SetInsertionPoint(BBMap_[BB]);
    for(auto InstIt = BB->begin(); InstIt != BB->end(); ++InstIt) {
      GenerateInstruction(*InstIt);
    }
  }

  ListScheduler Scheduler(MFunction_);
  Scheduler.Schedule();

  LinearScanRegAlloc RA(MFunction_);
  if(!RA.Allocate()) {
    throw std::runtime_error("Failed to allocate registers");
  }
  return;
}

void MachineFuncBuilder::GenerateBasicBlock(BasicBlock* BB) {
  std::stringstream SS;
  SS << "_" << Function_->Name() << "_bb" << BB->Index();
  std::string Name = SS.str();

  auto *MBB = CreateBlock(Name.c_str());
  SetInsertionPoint(MBB);
  BBMap_[BB] = MBB;
  return;
}

MachineOperand MachineFuncBuilder::ConvertOperand(const Operand& Op) {
  if(Op.IsRegister()) {
    if(VirtRegMap_.count(Op.RegId()) > 0) {
      return MachineOperand::CreateVirtualRegister(VirtRegMap_[Op.RegId()]);
    } else {
      auto MOp = NewReg();
      VirtRegMap_[Op.RegId()] = MOp.GetVirtualRegister();
      return MOp;
    }
  } else if(Op.IsImmediate()) {
    return MachineOperand::CreateImmediate(Op.Imm());
  }
  return MachineOperand::CreateMemory(MachineRegister::RBP, (Op.Param() + 2) * MachineOperand::WordSize());
}

void MachineFuncBuilder::HandleLogicalBinaryInst(BinaryInst& Inst) {
  auto Op = Inst.GetOperation();
  auto Dst = Inst.GetOut(0);
  auto Src1 = Inst.GetIn(0);
  auto Src2 = Inst.GetIn(1);

  if(Src1.IsImmediate()) {
    assert(!Src2.IsImmediate() && "Constant expressions should already be optimized");
    switch(Op) {
      case BinaryInst::Lt: {
        Op = BinaryInst::Gt;
        break;
      }
      case BinaryInst::Le: {
        Op = BinaryInst::Ge;
        break;
      }
      case BinaryInst::Gt: {
        Op = BinaryInst::Lt;
        break;
      }
      case BinaryInst::Ge: {
        Op = BinaryInst::Le;
        break;
      }
      default: {
        break;
      }
    }
    std::swap(Src1, Src2);
  }

  switch(Op) {
    case BinaryInst::Lt: {
      Xor(ConvertOperand(Dst), ConvertOperand(Dst));
      Cmp(ConvertOperand(Src2), ConvertOperand(Src1));
      CMov(MachineOperand::CreateImmediate(1), ConvertOperand(Dst), Condition::L);
      break; 
    }
    case BinaryInst::Le: {
      Xor(ConvertOperand(Dst), ConvertOperand(Dst));
      Cmp(ConvertOperand(Src2), ConvertOperand(Src1));
      CMov(MachineOperand::CreateImmediate(1), ConvertOperand(Dst), Condition::LE);
      break; 
    }
    case BinaryInst::Gt: {
      Xor(ConvertOperand(Dst), ConvertOperand(Dst));
      Cmp(ConvertOperand(Src2), ConvertOperand(Src1));
      CMov(MachineOperand::CreateImmediate(1), ConvertOperand(Dst), Condition::G);
      break; 
    }
    case BinaryInst::Ge: {
      Xor(ConvertOperand(Dst), ConvertOperand(Dst));
      Cmp(ConvertOperand(Src2), ConvertOperand(Src1));
      CMov(MachineOperand::CreateImmediate(1), ConvertOperand(Dst), Condition::GE);
      break; 
    }
    case BinaryInst::Eq: {
      Xor(ConvertOperand(Dst), ConvertOperand(Dst));
      Cmp(ConvertOperand(Src2), ConvertOperand(Src1));
      CMov(MachineOperand::CreateImmediate(1), ConvertOperand(Dst), Condition::E);
      break; 
    }
    case BinaryInst::Ne: {
      Xor(ConvertOperand(Dst), ConvertOperand(Dst));
      Cmp(ConvertOperand(Src2), ConvertOperand(Src1));
      CMov(MachineOperand::CreateImmediate(1), ConvertOperand(Dst), Condition::NE);
      break; 
    }
    default: {
      assert(false && "Should not reach here");
    }
  }
}

void MachineFuncBuilder::HandleBinaryInst(BinaryInst& Inst) {
  // TODO:
  auto Op = Inst.GetOperation();
  auto Dst = Inst.GetOut(0);
  auto Src1 = Inst.GetIn(0);
  auto Src2 = Inst.GetIn(1);

  switch(Op) {
    case BinaryInst::Add: {
      Mov(ConvertOperand(Src1), ConvertOperand(Dst));
      Add(ConvertOperand(Src2), ConvertOperand(Dst));
      break;
    }
    case BinaryInst::Sub: {
      Mov(ConvertOperand(Src1), ConvertOperand(Dst));
      Sub(ConvertOperand(Src2), ConvertOperand(Dst));
      break;
    }
    case BinaryInst::And: {
      Mov(ConvertOperand(Src1), ConvertOperand(Dst));
      And(ConvertOperand(Src2), ConvertOperand(Dst));
      break;
    }
    case BinaryInst::Or: {
      Mov(ConvertOperand(Src1), ConvertOperand(Dst));
      Or(ConvertOperand(Src2), ConvertOperand(Dst));
      break;
    }
    case BinaryInst::Xor: {
      Mov(ConvertOperand(Src1), ConvertOperand(Dst));
      Xor(ConvertOperand(Src2), ConvertOperand(Dst));
      break;
    }

    case BinaryInst::Mul: {
      Mov(ConvertOperand(Src1), ConvertOperand(Dst));
      if(Src2.IsImmediate()) {
        Mov(ConvertOperand(Src2), MachineOperand::CreateRegister(MachineRegister::RAX));
        IMul(MachineOperand::CreateRegister(MachineRegister::RAX), ConvertOperand(Dst));
      } else {
        IMul(ConvertOperand(Src2), ConvertOperand(Dst));
      }
      break; 
    }

    case BinaryInst::Div: {
      Mov(ConvertOperand(Src1), MachineOperand::CreateRegister(MachineRegister::RAX));
      Cqo();
      if(Src2.IsImmediate()) {
        auto TempReg = NewReg();
        Mov(ConvertOperand(Src2), TempReg);
        IDiv(TempReg);
      } else {
        IDiv(ConvertOperand(Src2));
      }
      Mov(MachineOperand::CreateRegister(MachineRegister::RAX), ConvertOperand(Dst));
      break;
    }

    case BinaryInst::Mod: {
      Mov(ConvertOperand(Src1), MachineOperand::CreateRegister(MachineRegister::RAX));
      Cqo();
      if(Src2.IsImmediate()) {
        auto TempReg = NewReg();
        Mov(ConvertOperand(Src2), TempReg);
        IDiv(TempReg);
      } else {
        IDiv(ConvertOperand(Src2));
      }
      Mov(MachineOperand::CreateRegister(MachineRegister::RDX), ConvertOperand(Dst));
      break;
    }

    case BinaryInst::Lt:
    case BinaryInst::Le:
    case BinaryInst::Gt:
    case BinaryInst::Ge:
    case BinaryInst::Eq:
    case BinaryInst::Ne: {
      HandleLogicalBinaryInst(Inst);
      break;
    }

    default: {
      assert(false && "Unhandled binary operation");
    }
  }
  return; 
}

void MachineFuncBuilder::GenerateInstruction(Instruction& Inst) {
  auto Type = Inst.Type();
  switch(Type) {
    // TODO: 
    case Instruction::Nop:
      break;
    case Instruction::Assign: {
      auto Src = Inst.GetIn(0);
      auto Dst = Inst.GetOut(0);
      Mov(ConvertOperand(Src), ConvertOperand(Dst));
      break;
    }
    case Instruction::Binary: {
      auto &BinI = static_cast<BinaryInst&>(Inst);
      HandleBinaryInst(BinI);
      break;
    }
    
    case Instruction::Jmp: {
      auto &JmpI = static_cast<JmpInst&>(Inst);
      Jmp(JmpI.Successor(0));
      break;
    }
    case Instruction::Jnz: {
      auto &JnzI = static_cast<JnzInst&>(Inst);
      
      auto Cond = JnzI.GetOperand(0);
      assert(!Cond.IsImmediate() && "Constant jump condition should already be optimized");

      auto *True = JnzI.Successor(0);
      auto *False = JnzI.Successor(1);

      Test(ConvertOperand(Cond), ConvertOperand(Cond));
      Jcc(True, False, Condition::NE);
      break;
    }
    case Instruction::RetVoid: {
      Ret();
      break;
    }
    case Instruction::Ret: {
      auto Op = Inst.GetOperand(0);
      Mov(ConvertOperand(Op), MachineOperand::CreateRegister(MachineRegister::RAX));
      Ret();
      break;
    }

    case Instruction::Call: {
      auto &CallI = static_cast<CallInst&>(Inst);
      for(int i = CallI.Ins() - 1; i >= 0; i--) {
        Push(ConvertOperand(CallI.GetIn(i)));
      }
      Call(CallI.Callee());

      if(CallI.Ins() > 0) {
        Add(MachineOperand::CreateImmediate(CallI.Ins() * MachineOperand::WordSize()), MachineOperand::CreateRegister(MachineRegister::RSP));
      }
      Mov(MachineOperand::CreateRegister(MachineRegister::RAX), ConvertOperand(CallI.GetOut(0)));
      break; 
    }
    case Instruction::CallVoid: {
      auto &CallI = static_cast<CallVoidInst&>(Inst);
      for(int i = CallI.Ins() - 1; i >= 0; i--) {
        Push(ConvertOperand(CallI.GetIn(i)));
      }
      Call(CallI.Callee());

      if(CallI.Ins() > 0) {
        Add(MachineOperand::CreateImmediate(CallI.Ins() * MachineOperand::WordSize()), MachineOperand::CreateRegister(MachineRegister::RSP));
      }
      break; 
    }

    case Instruction::LoadLabel: {
      auto &LoadI = static_cast<LoadLabelInst&>(Inst);
      Lea(LoadI.Label(), ConvertOperand(LoadI.GetOut(0)));
      break; 
    }
    case Instruction::ArrayLoad: {
      auto &ArrayLoadI = static_cast<ArrayLoadInst&>(Inst);
      auto RetVal = ConvertOperand(ArrayLoadI.GetOut(0));
      auto Array = ConvertOperand(ArrayLoadI.GetIn(0));
      auto Idx = ConvertOperand(ArrayLoadI.GetIn(1));

      Push(Idx);
      Push(Array);
      Call("array_load");
      Add(MachineOperand::CreateImmediate(2 * MachineOperand::WordSize()), MachineOperand::CreateRegister(MachineRegister::RSP));
      Mov(MachineOperand::CreateRegister(MachineRegister::RAX), RetVal);
      break;
    }
    case Instruction::ArrayStore: {
      auto &ArrayStoreI = static_cast<ArrayStoreInst&>(Inst);
      auto Array = ConvertOperand(ArrayStoreI.GetIn(0));
      auto Idx = ConvertOperand(ArrayStoreI.GetIn(1));
      auto Val = ConvertOperand(ArrayStoreI.GetIn(2));

      Push(Val);
      Push(Idx);
      Push(Array);
      Call("array_store");
      Add(MachineOperand::CreateImmediate(3 * MachineOperand::WordSize()), MachineOperand::CreateRegister(MachineRegister::RSP));
      break;
    }

    default: {
      assert(false && "Unhandled instruction type");
    }
  }
  return;
}

bool ModuleCodegen::Generate() {
  ModuleSS_ << ".intel_syntax noprefix\n";

  ModuleSS_ << ".text\n";
  for(auto *F : (*Module_)) {
    MachineFuncBuilder Builder(F);
    Builder.Generate();
    Builder.GetFunction()->Emit(ModuleSS_);
    ModuleSS_ << '\n';
  }

  ModuleSS_ << ".data\n";
  GenerateStringLiterals();
  return true; 
}

void ModuleCodegen::GenerateStringLiterals() {
  for(auto &KV : IRGenCtx_->StringLiterals) {
    ModuleSS_ << KV.second << ":\n";
    ModuleSS_ << ".byte ";
    for(size_t i = 0; i < KV.first.size(); ++i) {
      ModuleSS_ << static_cast<int>(KV.first[i]) << ", ";
    }
    ModuleSS_ << "0\n";
  }
}

bool ModuleCodegen::Save(const char* FileName) {
  FILE* F = fopen(FileName, "w");
  if(F == nullptr) {
    ERROR("Failed to open file %s", FileName);
    return false; 
  }

  auto S = ModuleSS_.str();
  if(fwrite(S.c_str(), 1, S.size(), F) != S.size()) {
    ERROR("Failed to write to file %s", FileName);
    fclose(F);
    return false;
  }
  fclose(F);
  return true;
}

} // namespace klang