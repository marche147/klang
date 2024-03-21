#include <IR/Optimize.h>
#include <IR/Analysis.h>

#include <Logging.h>

namespace klang {

#pragma region ConstPropagate
void ConstPropState::Meet(const ConstPropState& Other) {
  for(auto &KV : Other.State_) {
    auto MyValue = this->Get(KV.first);
    if(MyValue.State_ == kConstPropUndet) {
      State_[KV.first] = KV.second;
    } else if(KV.second.State_ == kConstPropUndet) {
      State_[KV.first] = MyValue;
    } else if(MyValue.State_ == kConstPropConstant && KV.second.State_ == kConstPropConstant) {
      if(MyValue.Value_ == KV.second.Value_) {
        State_[KV.first] = KV.second;
      } else {
        State_[KV.first].SetNonConstant();
      }
    } else {
      State_[KV.first].SetNonConstant();
    }
  }
}

void ConstPropState::Transfer(const Instruction& Inst) {
  auto Type = Inst.Type();

  // XXX: Update here if new instructions are added
  switch(Type) {
    case Instruction::Assign: {
      size_t Reg = Inst.GetOut(0).RegId();
      auto Value = this->FromOperand(Inst.GetIn(0));
      State_[Reg] = Value;
      break;
    }
    case Instruction::Binary: {
      auto &BinInst = dynamic_cast<const BinaryInst&>(Inst);
      size_t Reg = BinInst.GetOut(0).RegId();

      const auto &Op1 = BinInst.GetIn(0);
      const auto &Op2 = BinInst.GetIn(1);
      auto Value1 = this->FromOperand(Op1);
      auto Value2 = this->FromOperand(Op2);
      if(Value1.State_ == kConstPropNonConstant || Value2.State_ == kConstPropNonConstant) {
        State_[Reg].SetNonConstant();
      } else if(Value1.State_ == kConstPropConstant && Value2.State_ == kConstPropConstant) {
        auto Result = BinInst.Evaluate(BinInst.GetOperation(), Value1.Value_, Value2.Value_);
        State_[Reg].SetConstant(Result);
      } else {
        State_[Reg].SetUndet();
      }
    }

    case Instruction::Call: 
    case Instruction::ArrayNew:
    case Instruction::ArrayLoad:
    case Instruction::LoadLabel: {
      size_t Reg = Inst.GetOut(0).RegId();
      State_[Reg].SetNonConstant();
      break;
    }
    
    case Instruction::Ret:
    case Instruction::Nop:
    case Instruction::RetVoid:
    case Instruction::ArrayStore:
    case Instruction::Jmp:
    case Instruction::Jnz: 
    case Instruction::CallVoid: {
      break;
    }

    default: {
      assert(false && "unhandled instruction type");
    }
  }
  return; 
}

bool ConstantPropagate(Function* F) {
  bool Changed = false;
  auto [In, Out] = DataflowAnalysis<ConstPropState>(F);

  for(auto It = F->begin(); It != F->end(); It++) {
    auto *BB = *It;
    auto &State = Out[BB];

    for(auto InstIt = BB->begin(); InstIt != BB->end(); InstIt++) {
      for(size_t i = 0; i < InstIt->Ins(); i++) {
        if(InstIt->GetIn(i).IsRegister()) {
          size_t Reg = InstIt->GetIn(i).RegId();
          if(State.IsConstant(Reg)) {
            int64_t Value = State.GetConstant(Reg);
            InstIt->ReplaceIn(i, Operand::CreateImmediate(Value));
            Changed = true; 
          }
        }
      }
    }
  }
  return Changed;
}
#pragma endregion

#pragma region CopyPropagate
void CopyPropState::Meet(const CopyPropState& Other) {
  std::map<size_t, size_t> NewState; 
  for(auto &KV : State_) {
    auto It = Other.State_.find(KV.first);
    if(It != Other.State_.end() && It->second == KV.second) {
      NewState[KV.first] = KV.second;
    }
  }
  State_ = NewState;
}

void CopyPropState::Transfer(const Instruction& Inst) {
  if(Inst.Type() == Instruction::Assign) {
    auto In = Inst.GetIn(0);
    if(In.IsRegister()) {
      State_[Inst.GetOut(0).RegId()] = In.RegId();
    }
  } else {
    // invalidate all registers that are written to because
    // we don't know what the value is anymore
    for(size_t i = 0; i < Inst.Outs(); i++) {
      auto Op = Inst.GetOut(i);
      if(Op.IsRegister()) {
        State_.erase(Op.RegId());
      }
    }
  }
}

bool CopyPropagate(Function* F) {
  bool Changed = false;
  auto [In, Out] = DataflowAnalysis<CopyPropState>(F);

  for(auto It = F->begin(); It != F->end(); It++) {
    auto *BB = *It;
    auto &State = In[BB];
    for(auto InstIt = BB->begin(); InstIt != BB->end(); InstIt++) {
      for(size_t i = 0; i < InstIt->Ins(); i++) {
        auto Op = InstIt->GetIn(i);
        if(Op.IsRegister() && State.Get(Op.RegId()) != std::nullopt) {
          InstIt->ReplaceIn(i, Operand::CreateRegister(State.Get(Op.RegId()).value()));
          Changed = true;
        }
      }
      
      // update state within the block
      State.Transfer(*InstIt);
    }
  }
  return Changed;
}
#pragma endregion

#pragma region CommonSubexpressionElimination
std::optional<CSEValue> CSEValue::FromInstruction(const Instruction& Inst) {
  if(Inst.Type() == Instruction::Binary) {
    auto &BinInst = dynamic_cast<const BinaryInst&>(Inst);
    auto Op1 = BinInst.GetIn(0);
    auto Op2 = BinInst.GetIn(1);

    // RHS should be immutable
    if((Op1.IsImmediate() || Op1.IsParameter()) && (Op2.IsImmediate() || Op2.IsParameter())) {
      return std::make_optional(CSEValue(BinInst.GetOperation(), Op1, Op2));
    }
  }
  return std::nullopt;
} 

bool GCSEState::Contains(const CSEValue& Value) const {
  if(!Init_) {
    return false;
  }
  return Values_.count(Value) != 0;
}

void GCSEState::Intersect(const GCSEState& Other) {
  if(!Other.Init_) {
    return;
  }

  if(!Init_) {
    Values_ = std::set(Other.Values_.begin(), Other.Values_.end());
    Init_ = true;
    return;
  }

  std::set<CSEValue> NewValues;
  for(auto &Value : Values_) {
    if(Other.Contains(Value)) {
      NewValues.insert(Value);
    }
  }
  Values_ = NewValues;
  return ;
}

void GCSEState::Meet(const GCSEState& Other) {
  Intersect(Other);
}

void GCSEState::Transfer(const Instruction& Inst) {
  auto Value = CSEValue::FromInstruction(Inst);
  if(Value.has_value()) {
    Values_.insert(Value.value());
    Init_ = true;
  }
}

static bool LocalCSEBlock(BasicBlock* BB) {
  bool Changed = false;
  int i = 0;

  std::map<CSEValue, int> Mapping;
  std::map<CSEValue, Instruction*> Defs;
  std::map<int, std::set<int>> State;
  std::map<CSEValue, std::vector<Instruction*>> Reusable;

  // find all reusable expressions
  State[0] = std::set<int>();
  for(auto InstIt = BB->begin(); InstIt != BB->end(); InstIt++) {
    auto &Inst = *InstIt;
    auto Value = CSEValue::FromInstruction(Inst);
    if(Value.has_value()) {
      auto Expr = Value.value();
      if(Mapping.count(Expr) == 0) {
        Mapping[Expr] = i;
        Defs[Expr] = &Inst;
      } else {
        auto Id = Mapping[Expr];
        if(State[i].count(Id) != 0) {
          Reusable[Expr].push_back(&Inst);
        } 
      }
      State[i].insert(Mapping[Expr]);
    }
    State[i + 1] = State[i];
    i++;
  }

  for(auto &KV : Reusable) {
    if(KV.second.size() != 0) {
      auto *Def = Defs[KV.first];
      auto NewReg = Operand::CreateRegister(BB->Parent()->NewReg());
      auto OldReg = Def->GetOut(0);
      Def->ReplaceOut(0, NewReg);
      Def->Parent()->InsertAfter(new AssignInst(OldReg, NewReg), Def);

      for(auto *User : KV.second) {
        auto Dst = User->GetOut(0);
        User->Parent()->Replace(new AssignInst(Dst, NewReg), User);
        delete User;
      }
      Changed = true;
    }
  }
  return Changed;
}

bool LocalCSE(Function* F) {
  bool Changed = false;
  for(auto *BB : (*F)) {
    Changed |= LocalCSEBlock(BB);
  }
  return Changed;
}

static void ReplaceLHS(BasicBlock* Start, BasicBlock* Current, const CSEValue& Value, Operand NewReg, std::set<BasicBlock*>& Visited) {
  if(Visited.count(Current)) {
    return;
  }
  Visited.insert(Current);

  if(Start != Current) {
    for(auto InstIt = Current->begin(); InstIt != Current->end(); InstIt++) {
      auto &Inst = *InstIt;
      auto Value = CSEValue::FromInstruction(Inst);
      if(Value.has_value() && Value.value() == Value) {
        auto OldReg = Inst.GetOut(0);
        Inst.ReplaceOut(0, NewReg);
        Inst.Parent()->InsertAfter(new AssignInst(OldReg, NewReg), &Inst);
      }
    }
  }

  for(auto *Pred : Current->Predecessors()) {
    ReplaceLHS(Start, Pred, Value, NewReg, Visited);
  }
  return; 
}

static bool GlobalCSEBlock(BasicBlock* BB, const GCSEState& State) {
  bool Changed = false;

  for(auto InstIt = BB->begin(); InstIt != BB->end(); InstIt++) {
    auto &Inst = *InstIt;
    auto *InstPtr = &Inst;
    auto Value = CSEValue::FromInstruction(Inst);
    if(Value.has_value() && State.Contains(Value.value())) {
      auto NewReg = Operand::CreateRegister(BB->Parent()->NewReg());
      std::set<BasicBlock*> Visited;
      
      // Replace exisiting expressions
      ReplaceLHS(BB, BB, Value.value(), NewReg, Visited);

      auto Dst = Inst.GetOut(0);
      InstPtr->Parent()->Replace(new AssignInst(Dst, NewReg), InstPtr);
      delete InstPtr;

      Changed = true;
    }
  }
  return Changed;
}

bool GlobalCSE(Function* F) {
  bool Changed = false;
  auto [In, Out] = DataflowAnalysis<GCSEState>(F);

  for(auto *BB : (*F)) {
    Changed |= GlobalCSEBlock(BB, In[BB]);
  }
  return Changed;
}
#pragma endregion

#pragma region DeadCodeElimination
static bool RewriteConstantJump(BasicBlock* BB) {
  bool Changed = false;
  auto &Term = *BB->rbegin();
  if(Term.Type() == Instruction::Jnz) {
    auto &Jnz = static_cast<JnzInst&>(Term);
    auto Cond = Jnz.GetIn(0);
    if(Cond.IsImmediate()) {
      auto *Branch = Cond.Imm() != 0 ? Jnz.Successor(0) : Jnz.Successor(1);
      auto *JnzPtr = &Jnz;
      JnzPtr->Parent()->Replace(new JmpInst(Branch), JnzPtr);
      delete JnzPtr;
      Changed = true;
    }
  }
  return Changed; 
}

static bool RewriteConstantBinary(BasicBlock* BB) {
  bool Changed = false;
  for(auto InstIt = BB->begin(); InstIt != BB->end(); InstIt++) {
    auto &Inst = *InstIt;
    if(Inst.Type() == Instruction::Binary) {
      auto &BinInst = static_cast<BinaryInst&>(Inst);
      auto Op1 = BinInst.GetIn(0);
      auto Op2 = BinInst.GetIn(1);
      if(Op1.IsImmediate() && Op2.IsImmediate()) {
        auto Result = BinInst.Evaluate(BinInst.GetOperation(), Op1.Imm(), Op2.Imm());
        auto *BinInstPtr = &BinInst;
        BinInstPtr->Parent()->Replace(new AssignInst(BinInst.GetOut(0), Operand::CreateImmediate(Result)), BinInstPtr);
        delete BinInstPtr;
        Changed = true;
      }
    }
  }
  return Changed;
}

static bool RemoveDeadBlocks(Function* F) {
  bool Changed = false;

  for(auto *BB : (*F)) {
    Changed |= RewriteConstantBinary(BB);
  }

  for(auto *BB : (*F)) {
    Changed |= RewriteConstantJump(BB);
  }
  
  std::vector<BasicBlock*> DeadBlocks;
  for(auto *BB : (*F)) {
    if(BB->Predecessors().size() == 0 && F->Entry() != BB) {
      DeadBlocks.push_back(BB);
    }
  }

  for(auto *BB : DeadBlocks) {
    F->Remove(BB);
    delete BB;
    Changed = true;
  }

  return Changed; 
}

static bool IsDummyInstruction(const Instruction& Inst) {
  if(Inst.Type() == Instruction::Nop) {
    return true;
  } else if(Inst.Type() == Instruction::Assign) {
    auto &Assign = static_cast<const AssignInst&>(Inst);
    return Assign.GetIn(0).IsRegister() && Assign.GetOut(0).RegId() == Assign.GetIn(0).RegId();
  }
  return false;
} 

static bool RemoveDummyInstruction(Function* F) {
  bool Changed = false;
  for(auto *BB : (*F)) {
    for(auto InstIt = BB->begin(); InstIt != BB->end(); InstIt++) {
      auto &Inst = *InstIt;
      if(IsDummyInstruction(Inst)) {
        auto *InstPtr = &Inst;
        InstPtr->Parent()->Remove(InstPtr);
        delete InstPtr;
        Changed = true;
      }
    }
  }
  return Changed; 
}

void LivenessState::Meet(const LivenessState& Other) {
  for(auto Reg : Other.LiveRegs_) {
    LiveRegs_.insert(Reg);
  }
}

void LivenessState::Transfer(const Instruction& Inst) {
  for(size_t i = 0; i < Inst.Outs(); i++) {
    auto Op = Inst.GetOut(i);
    if(Op.IsRegister() && LiveRegs_.count(Op.RegId()) != 0) {
      LiveRegs_.erase(Op.RegId());
    }
  }

  for(size_t i = 0; i < Inst.Ins(); i++) {
    auto Op = Inst.GetIn(i);
    if(Op.IsRegister()) {
      LiveRegs_.insert(Op.RegId());
    }
  }
}

static bool DeadVariableEliminationBlock(BasicBlock* BB, const LivenessState& StateIn, const LivenessState& StateOut) {
  bool Changed = false;

  std::map<size_t, Instruction*> LastDefs;
  std::map<Instruction*, std::vector<Instruction*>> DefsToUses;
  std::map<Instruction*, std::vector<Instruction*>> UsesToDefs;

  for(auto InstIt = BB->begin(); InstIt != BB->end(); InstIt++) {
    auto &Inst = *InstIt;

    if(Inst.Ins() > 0) {
      for(size_t i = 0; i < Inst.Ins(); i++) {
        auto Op = Inst.GetIn(i);
        if(Op.IsRegister()) {
          if(LastDefs.count(Op.RegId()) != 0) {
            DefsToUses[LastDefs[Op.RegId()]].push_back(&Inst);
            UsesToDefs[&Inst].push_back(LastDefs[Op.RegId()]);
          } else {
            assert(StateIn.Contains(Op.RegId()) && "Use of undefined dead register");
          }
        }
      }
    }

    // update defs 
    if(Inst.Outs() == 1) {
      auto Out = Inst.GetOut(0);
      if(Out.IsRegister()) {
        LastDefs[Out.RegId()] = &Inst;
      }
    }
  }

  // collect needed instructions
  std::set<Instruction*> Needed;

  // recursive function to add all needed defs and their uses
  std::function<void(Instruction*)> AddAllToNeeded = [&](Instruction* Inst) {
    Needed.insert(Inst);
    for(auto *NeededDefs : UsesToDefs[Inst]) {
      AddAllToNeeded(NeededDefs);
    }
  };

  if(!BB->IsExit()) {
    // Liveout registers are needed
    for(auto Reg : StateOut.LiveRegs_) {
      if(LastDefs.count(Reg) != 0) {
        AddAllToNeeded(LastDefs[Reg]);
      }
    }
  }
  
  if(BB->rbegin()->Type() == Instruction::Ret) {
    // if "Ret x" is the last instruction, then x is needed
    auto &Ret = static_cast<RetInst&>(*BB->rbegin());
    if(Ret.GetIn(0).IsRegister()) {
      AddAllToNeeded(LastDefs[Ret.GetIn(0).RegId()]);
    }
  } else if(BB->rbegin()->Type() == Instruction::Jnz) {
    // if "Jnz x, a, b" is the last instruction, then x is needed
    auto &Jnz = static_cast<JnzInst&>(*BB->rbegin());
    if(Jnz.GetIn(0).IsRegister()) {
      AddAllToNeeded(LastDefs[Jnz.GetIn(0).RegId()]);
    }
  }

  // Add other needed operands
  for(auto InstIt = BB->begin(); InstIt != BB->end(); InstIt++) {
    auto &Inst = *InstIt;
    if(Inst.Type() == Instruction::Call 
    || Inst.Type() == Instruction::CallVoid 
    || Inst.Type() == Instruction::ArrayStore) {
      for(size_t i = 0; i < Inst.Ins(); i++) {
        auto Op = Inst.GetIn(i);
        if(Op.IsRegister()) {
          AddAllToNeeded(LastDefs[Op.RegId()]);
        }
      }
    }
  }

  // eliminate dead instructions
  for(auto InstIt = BB->begin(); InstIt != BB->end(); InstIt++) {
    auto &Inst = *InstIt;
    if(Needed.count(&Inst) == 0 && !Inst.HasSideEffects()) {
      auto *InstPtr = &Inst;
      InstPtr->Parent()->Remove(InstPtr);
      delete InstPtr;
      Changed = true;
    }
  }
  return Changed; 
}

static bool DeadVariableElimination(Function* F) {
  bool Changed = false;
  auto [In, Out] = DataflowAnalysis<LivenessState, false>(F);

  for(auto *BB : (*F)) {
    Changed |= DeadVariableEliminationBlock(BB, In[BB], Out[BB]);
  }
  return Changed;
}

bool DeadCodeElimination(Function* F) {
  bool Changed = false;
  
  Changed |= DeadVariableElimination(F);
  Changed |= RemoveDummyInstruction(F);
  Changed |= RemoveDeadBlocks(F);
  return Changed; 
}
#pragma endregion

void OptimizeIR(Function* F) {
  bool Changed;
  do {
    Changed = false;
    Changed |= ConstantPropagate(F);
    Changed |= CopyPropagate(F);
    Changed |= LocalCSE(F);
    Changed |= GlobalCSE(F);
    Changed |= DeadCodeElimination(F);
  } while(Changed);
  return;
}

} // namespace klang