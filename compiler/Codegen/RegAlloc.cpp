#include <Codegen/RegAlloc.h>
#include <IR/Analysis.h>
#include <Logging.h>

// https://www.cs.rice.edu/~kvp1/spring2008/lecture7.pdf
// XXX: Refine handling for intervals using dataflow analysis 

namespace klang {

void MRegLivenessState::Meet(const MRegLivenessState& Other) {
  for(auto Reg : Other.Live_) {
    Live_.insert(Reg);
  }
}

void MRegLivenessState::Transfer(const MachineInstruction* Inst) {
  auto Op = Inst->GetOpcode();
  switch(Op) {
    case MachineInstruction::Opcode::Mov:
    case MachineInstruction::Opcode::CMov:
    case MachineInstruction::Opcode::Add: 
    case MachineInstruction::Opcode::Sub:
    case MachineInstruction::Opcode::IMul: 
    case MachineInstruction::Opcode::And: 
    case MachineInstruction::Opcode::Or: 
    case MachineInstruction::Opcode::Xor: {
      auto Src = Inst->GetOperand(0);
      auto Dst = Inst->GetOperand(1);
      if(Dst.IsVirtualRegister()) {
        Live_.erase(Dst.GetVirtualRegister());
      }
      if(Src.IsVirtualRegister()) {
        Live_.insert(Src.GetVirtualRegister());
      }
      break;
    }

    case MachineInstruction::Opcode::Cmp:
    case MachineInstruction::Opcode::Test: {
      auto Src1 = Inst->GetOperand(0);
      auto Src2 = Inst->GetOperand(1);
      if(Src1.IsVirtualRegister()) {
        Live_.insert(Src1.GetVirtualRegister());
      }
      if(Src2.IsVirtualRegister()) {
        Live_.insert(Src2.GetVirtualRegister());
      }
      break;
    }

    case MachineInstruction::Opcode::Jmp:
    case MachineInstruction::Opcode::Jcc:
    case MachineInstruction::Opcode::Ret:
    case MachineInstruction::Opcode::Call: 
    case MachineInstruction::Opcode::Cqo: {
      break;
    }

    case MachineInstruction::Opcode::Push:
    case MachineInstruction::Opcode::IDiv: {
      auto Src = Inst->GetOperand(0);
      if(Src.IsVirtualRegister()) {
        Live_.insert(Src.GetVirtualRegister());
      }
      break;
    }
    case MachineInstruction::Opcode::Pop:
    case MachineInstruction::Opcode::Lea: {
      auto Dst = Inst->GetOperand(0);
      if(Dst.IsVirtualRegister()) {
        Live_.erase(Dst.GetVirtualRegister());
      }
      break;
    }

    default: assert(false && "Unhandled instruction");
  }
}

std::vector<MachineBasicBlock*> LinearScanRegAlloc::SortBlocks() {
  auto Blocks = Func_->PostOrder();
  return std::vector(Blocks.rbegin(), Blocks.rend());
}

std::vector<Interval*> LinearScanRegAlloc::ComputeInterval(const std::vector<MachineBasicBlock*>& Blocks) {
  std::vector<Interval*> Intervals;

  for(auto *BB : Blocks) {
#if DEBUG_REGALLOC
    DEBUG("Computing intervals for block %s\n", BB->Name());
#endif 
    for(auto InstIt = BB->begin(); InstIt != BB->end(); InstIt++) {
      auto *Inst = *InstIt;
      ComputeIntervalSingle(Blocks, Inst, Intervals);
    }
  }
  return Intervals;
}

bool LinearScanRegAlloc::InstructionInLoop(MachineInstruction* Inst) {
  auto *Start = Inst->Parent();
  bool Result = false;
  std::set<MachineBasicBlock*> Visited;

  std::function<void(MachineBasicBlock*)> Visitor = [&](MachineBasicBlock* BB) {
    if(Visited.count(BB) > 0) {
      return;
    }
    Visited.insert(BB);
    if(BB == Start) {
      Result = true;
      return;
    }
    for(auto *Succ : BB->Successors()) {
      Visitor(Succ);
    }
  };

  for(auto *Succ : Start->Successors()) {
    Visitor(Succ);
  }
  return Result;
}

std::pair<MachineBasicBlock*, MachineBasicBlock*> LinearScanRegAlloc::FindLoopEntryExitBlock(MachineBasicBlock* Block) {
  std::set<MachineBasicBlock*> Visited;
  MachineBasicBlock* Entry = nullptr; 
  MachineBasicBlock* Exit = nullptr;

  std::function<void(MachineBasicBlock*)> Visitor = [&](MachineBasicBlock* Current) {
    if(Visited.count(Current) > 0) {
      return;
    }
    Visited.insert(Current);

    for(auto *Succ : Current->Successors()) {
      auto Next = Succ->Successors();
      if(Next.size() == 2) {
        int Start = InstToOrder_[*(Succ->begin())];
        int A = InstToOrder_[*(Next[0]->begin())];
        int B = InstToOrder_[*(Next[1]->begin())];
        if(A <= Start && Start <= B) {
          Entry = Next[0];
          Exit = Succ;
        } else if(B <= Start && Start <= A) {
          Entry = Next[1];
          Exit = Succ;
        }
      }
      Visitor(Succ);
    }
  };

  Visitor(Block);
  return std::make_pair(Entry, Exit);
}

void LinearScanRegAlloc::ComputeIntervalSingle(const std::vector<MachineBasicBlock*>& Blocks, MachineInstruction* Inst, std::vector<Interval*>& Intervals) {
  int Start = InstToOrder_[Inst];
  int End = -1;   // Compute end
  auto [In, Out] = MFDataflowAnalysis<MRegLivenessState, false>(Func_);

  for(size_t i = 0; i < Inst->Size(); i++) {
    auto Op = Inst->GetOperand(i);
    if(Op.IsVirtualRegister() && VirtRegToInterval_.count(Op.GetVirtualRegister()) == 0) {
      // Search for the last use of the virtual register
      int Current = Start;
      while(OrderToInst_.count(Current) > 0) {
        auto *CurrentInst = OrderToInst_[Current];
        for(size_t j = 0; j < CurrentInst->Size(); j++) {
          auto CurrentOp = CurrentInst->GetOperand(j);
          if(CurrentOp.IsVirtualRegister() && CurrentOp.GetVirtualRegister() == Op.GetVirtualRegister()) {
            End = Current;
          }
        }
        Current++;
      }

#if DEBUG_REGALLOC
      DEBUG("Linear scan register interval for virt reg %d: [%d, %d]\n", Op.GetVirtualRegister(), Start, End);
#endif 

#if 0
      // XXX: nested loops are not supported
      auto *StartInst = OrderToInst_[Start];
      if(InstructionInLoop(StartInst)) {
        auto [Entry, Exit] = FindLoopEntryExitBlock(StartInst->Parent());
#if DEBUG_REGALLOC
        DEBUG("Found loop entry and exit block for instruction %s: %s, %s\n", StartInst->Parent()->Name(), Entry->Name(), Exit->Name());
#endif 
        int EStart = InstToOrder_[*Entry->begin()];
        assert(EStart <= Start && "loop entry should be before the start of the interval");
        Start = EStart;
      }

      auto *EndInst = OrderToInst_[End];
      if(InstructionInLoop(EndInst)) {
        auto [Entry, Exit] = FindLoopEntryExitBlock(EndInst->Parent());
#if DEBUG_REGALLOC
        DEBUG("Found loop entry and exit block for instruction %s: %s, %s\n", EndInst->Parent()->Name(), Entry->Name(), Exit->Name());
#endif 
        int EEnd = InstToOrder_[*Exit->rbegin()];
        assert(EEnd >= End && "loop exit should be after the end of the interval");
        End = EEnd;
      }
#endif 

      // XXX: determine whether the interval needs
      // an extension
      for(auto *BB : Blocks) {
        auto OutState = Out[BB];
        if(OutState.Contains(Op.GetVirtualRegister())) {
          int BBEnd = InstToOrder_[*BB->rbegin()];
          if(BBEnd > End) {
            DEBUG("Extending interval end for virtual register %d: [%d, %d] -> [%d, %d]\n", Op.GetVirtualRegister(), Start, End, Start, BBEnd);
            End = BBEnd;
          }
        }

        auto InState = In[BB];
        if(InState.Contains(Op.GetVirtualRegister())) {
          int BBStart = InstToOrder_[*BB->begin()];
          if(BBStart < Start) {
            DEBUG("Extending interval start for virtual register %d: [%d, %d] -> [%d, %d]\n", Op.GetVirtualRegister(), Start, End, BBStart, End);
            Start = BBStart;
          }
        }
      }

#if DEBUG_REGALLOC
      DEBUG("Found interval for virtual register %d: [%d, %d]\n", Op.GetVirtualRegister(), Start, End);
#endif
      auto *NewInterval = new Interval(Op.GetVirtualRegister(), Start, End);
      VirtRegToInterval_[Op.GetVirtualRegister()] = NewInterval;
      Intervals.push_back(NewInterval);
    }
  }
}

static void ReplaceVirtualRegister(MachineInstruction* Inst, size_t VirtRegId, MachineRegister New) {
  for(size_t i = 0; i < Inst->Size(); i++) {
    auto Op = Inst->GetOperand(i);
    if(Op.IsVirtualRegister() && Op.GetVirtualRegister() == VirtRegId) {
      Inst->ReplaceOperand(i, MachineOperand::CreateRegister(New));
    }
  }
}

static void ReplaceVirtualRegister(MachineInstruction* Inst, size_t VirtRegId, MachineOperand New) {
  for(size_t i = 0; i < Inst->Size(); i++) {
    auto Op = Inst->GetOperand(i);
    if(Op.IsVirtualRegister() && Op.GetVirtualRegister() == VirtRegId) {
      Inst->ReplaceOperand(i, New);
    }
  }
}

void LinearScanRegAlloc::FixupCallInst(MachineInstruction* Inst, std::vector<Interval*>& Intervals) {
  int Order = InstToOrder_[Inst];

  // find active registers used at the call site
  std::set<Interval*> ActiveAtCall;
  for(auto *I : Intervals) {
    int Start = I->Start();
    int RealEnd = I->End();
    if(I->IsSpilled()) {
      RealEnd = I->SpillAt();
    }

    if(Order >= Start && Order <= RealEnd) {
#if DEBUG_REGALLOC
      DEBUG("Found active interval [%d, %d] at call site (%d)\n", Start, I->End(), Order);
#endif 
      ActiveAtCall.insert(I);
    }
  }

  // need to save active registers
  for(auto *I : ActiveAtCall) {
    if(I->Reg() != None) {
      int Spill = AllocateSpillSlot(I);
      auto SpillSlot = MachineOperand::CreateMemory(RBP, -(Spill + 1) * MachineOperand::WordSize());
      Inst->Parent()->InsertBefore(new MovMachineInst(
        MachineOperand::CreateRegister(I->Reg()), 
        SpillSlot
      ), Inst);
      Inst->Parent()->InsertAfter(new MovMachineInst(
        SpillSlot,
        MachineOperand::CreateRegister(I->Reg())
      ), Inst);
    }
  }
}

// XXX: Currently the problem is that we cannot properly handle loops.
//
bool LinearScanRegAlloc::Allocate() {
  // Sort blocks by reverse post order
  auto Blocks = SortBlocks();

#if DEBUG_REGALLOC
  std::stringstream SS;
  DEBUG("Sorted %d blocks\n", Blocks.size());
#endif 

  // Label each instruction with increasing order
  int Order = 0;
  for(auto *BB : Blocks) {
    for(auto InstIt = BB->begin(); InstIt != BB->end(); ++InstIt) {
      OrderToInst_[Order] = *InstIt;
      InstToOrder_[*InstIt] = Order++;
    }
  }

#if DEBUG_REGALLOC
  DumpOrderedInstructions();
#endif 

  // Compute live intervals
  auto Intervals = ComputeInterval(Blocks);

#if DEBUG_REGALLOC
  DEBUG("Found %d live intervals\n", Intervals.size());
#endif 

  // Linear scan
  std::vector<Interval*> ByStart(Intervals.begin(), Intervals.end());
  std::sort(ByStart.begin(), ByStart.end(), [](Interval* A, Interval* B) {
    return A->Start() < B->Start();
  });

  std::vector<Interval*> Active;
  std::unordered_map<Interval*, MachineRegister> ActiveToRegister;
  std::set<MachineRegister> FreeRegisters;

  constexpr MachineRegister Allocatables[] = { RCX, R8, R9, R10, R11, RSI, RDI };
  constexpr size_t kAllocatableRegisters = sizeof(Allocatables) / sizeof(Allocatables[0]);
  for(auto Reg : Allocatables) {
    FreeRegisters.insert(Reg);
  }

  auto ExpireOldIntervals = [&](Interval* Current) {
    for(size_t i = 0; i < Active.size(); i++) {
      if(Active[i]->End() >= Current->Start()) {
        return; 
      }
      auto Reg = ActiveToRegister[Active[i]];
      ActiveToRegister.erase(Active[i]);
      FreeRegisters.insert(Reg);
      Active.erase(Active.begin() + i);
    }
  };

  auto AddActive = [&](Interval* Current) {
    for(size_t i = 0; i < Active.size(); i++) {
      if(Active[i]->End() > Current->End()) {
        Active.insert(Active.begin() + i, Current);
        return;
      }
    }
    Active.push_back(Current);
  };

  auto AllocateFreeRegister = [&](Interval* Current) {
    auto Reg = *FreeRegisters.begin();
    FreeRegisters.erase(Reg);
    ActiveToRegister[Current] = Reg;
  };

  auto SpillAtInterval = [&](Interval* Current) {
    auto *Last = Active.back();
    if(Last->End() > Current->End()) {
      auto Reg = ActiveToRegister[Last];
      ActiveToRegister.erase(Last);
      Active.pop_back();

      ActiveToRegister[Current] = Reg;
      Current->SetReg(Reg);
      AddActive(Current);
      return std::make_tuple(Last, Reg);
    }
    return std::make_tuple(Current, None);
  };

  for(auto *I : ByStart) {
    ExpireOldIntervals(I);
    if(Active.size() == kAllocatableRegisters) {
      auto [Spilled, _] = SpillAtInterval(I);
      auto Slot = AllocateSpillSlot(Spilled);
      Spilled->SpillAt(I->Start(), Slot);
#if DEBUG_REGALLOC
      DEBUG("Spilling interval (%d)[%d, %d] to slot %d\n", Spilled->VirtRegId(), Spilled->Start(), Spilled->End(), Slot);
      DEBUG("Current interval (%d)[%d, %d]\n", I->VirtRegId(), I->Start(), I->End());
      if(I == Spilled) {
        DEBUG("Spilled interval is the same as the current interval\n");
      } else {
        DEBUG("Spilled interval is different from the current interval\n");
      }
#endif 
    } else {
      AllocateFreeRegister(I);
      I->SetReg(ActiveToRegister[I]);
      AddActive(I);
#if DEBUG_REGALLOC
      DEBUG("Allocating register %s to interval (%d)[%d, %d]\n", GetRegisterName(I->Reg()), I->VirtRegId(), I->Start(), I->End());
#endif
    }
  }

#if DEBUG_REGALLOC
  SS.str("");
  Func_->Emit(SS);
  DEBUG("Original:\n%s\n", SS.str().c_str());
#endif 

  // Rewrite code to use physical registers
  // XXX: This is not how you handle spills :<
  for(auto *I : Intervals) {
    if(!I->IsSpilled()) {
      // Simply replace virtual register with physical register
#if DEBUG_REGALLOC
      DEBUG("Replacing virtual register %d [%d, %d] with physical register %s\n", I->VirtRegId(), I->Start(), I->End(), GetRegisterName(I->Reg()));
#endif 
      assert(I->Reg() != None && "Interval should have a register assigned");
      for(int i = I->Start(); i <= I->End(); i++) {
        ReplaceVirtualRegister(OrderToInst_[i], I->VirtRegId(), I->Reg());
      }
    } else {
      // XXX: Use allocated register before spilling.
      auto SpillSlot = MachineOperand::CreateMemory(RBP, -(I->SpillSlot() + 1) * MachineOperand::WordSize());

#ifdef DEBUG_REGALLOC
      DEBUG("Spilling interval (%d)[%d, %d] to slot %d at %d\n", I->VirtRegId(), I->Start(), I->End(), I->SpillSlot(), I->SpillAt());
#endif 

      for(int i = I->Start(); i < I->SpillAt(); i++) {
        ReplaceVirtualRegister(OrderToInst_[i], I->VirtRegId(), I->Reg());
      }

      // Spill
      if(I->Reg() != None) {
        // VirtReg was in physical register before spilling
        auto *SpillAtInst = OrderToInst_[I->SpillAt()];
        SpillAtInst->Parent()->InsertBefore(new MovMachineInst(MachineOperand::CreateRegister(I->Reg()), SpillSlot), SpillAtInst);
      }

      // Use memory for the rest of the interval
      for(int i = I->SpillAt(); i <= I->End(); i++) {
        ReplaceVirtualRegister(OrderToInst_[i], I->VirtRegId(), SpillSlot);
      }
    }
  }

#if DEBUG_REGALLOC
  SS.str("");
  Func_->Emit(SS);
  DEBUG("Rewritten:\n%s\n", SS.str().c_str());
#endif 

  // needs fixup for instructions with 2 or more memory operands
  FixupInstruction(Func_);

#if DEBUG_REGALLOC
  SS.str("");
  Func_->Emit(SS);
  DEBUG("Rewritten after fixup:\n%s\n", SS.str().c_str());
#endif 

  // Fixup call instructions
  for(auto KV : InstToOrder_) {
    auto *Inst = KV.first;
    if(Inst->GetOpcode() == MachineInstruction::Opcode::Call) {
      FixupCallInst(Inst, Intervals);
    }
  }

  // Emit prologue and epilogue
  EmitPrologue();
  EmitEpilogue();
  return true;
}

void LinearScanRegAlloc::EmitPrologue() {
  auto *Entry = Func_->Entry();
  auto *First = *Entry->begin();
  Entry->InsertBefore(new PushMachineInst(MachineOperand::CreateRegister(RBP)), First);
  Entry->InsertBefore(new MovMachineInst(MachineOperand::CreateRegister(RSP), MachineOperand::CreateRegister(RBP)), First);

  if(SpilledIntervals_.empty()) {
    return;
  }
  Entry->InsertBefore(new SubMachineInst(
    MachineOperand::CreateImmediate(MachineOperand::WordSize() * SpilledIntervals_.size()), 
    MachineOperand::CreateRegister(RSP)
  ), First);
}

void LinearScanRegAlloc::EmitEpilogue() {
  for(auto *BB : (*Func_)) {
    if(BB->IsExit()) {
      auto *Last = *BB->rbegin();
      BB->InsertBefore(new MovMachineInst(MachineOperand::CreateRegister(RBP), MachineOperand::CreateRegister(RSP)), Last);
      BB->InsertBefore(new PopMachineInst(MachineOperand::CreateRegister(RBP)), Last);
    }
  }
}

static bool Is64BitImmediate(int64_t Value) {
  return !(Value >= INT32_MIN && Value <= INT32_MAX);
}

bool FixupInstruction(MachineFunction* F) {
  for(auto *BB : (*F)) {
    for(auto InstIt = BB->begin(); InstIt != BB->end(); InstIt++) {
      auto *Inst = *InstIt;
      switch(Inst->GetOpcode()) {
        case MachineInstruction::Opcode::Mov: {
          auto Src = Inst->GetOperand(0);
          auto Dst = Inst->GetOperand(1);
          if(Src.IsMemory() && Dst.IsMemory()) {
            Inst->Parent()->InsertBefore(new MovMachineInst(Src, MachineOperand::CreateRegister(RAX)), Inst);
            Inst->ReplaceOperand(0, MachineOperand::CreateRegister(RAX));
          } else if(Dst.IsMemory() && Src.IsImmediate()) {
            Inst->Parent()->InsertBefore(new MovMachineInst(Src, MachineOperand::CreateRegister(RAX)), Inst);
            Inst->ReplaceOperand(0, MachineOperand::CreateRegister(RAX));
          }
          break; 
        }
        case MachineInstruction::Opcode::Add: 
        case MachineInstruction::Opcode::Sub:
        case MachineInstruction::Opcode::And: 
        case MachineInstruction::Opcode::Or: 
        case MachineInstruction::Opcode::Xor: 
        case MachineInstruction::Opcode::Cmp: 
        case MachineInstruction::Opcode::Test: {
          auto Src = Inst->GetOperand(0);
          auto Dst = Inst->GetOperand(1);
          if(Src.IsMemory() && Dst.IsMemory()) {
            Inst->Parent()->InsertBefore(new MovMachineInst(Src, MachineOperand::CreateRegister(RAX)), Inst);
            Inst->ReplaceOperand(0, MachineOperand::CreateRegister(RAX));
          } else if(Src.IsImmediate() && Is64BitImmediate(Src.GetImmediate())) {
            // Operation does not support 64-bit embedded immediate
            Inst->Parent()->InsertBefore(new MovMachineInst(Src, MachineOperand::CreateRegister(RAX)), Inst);
            Inst->ReplaceOperand(0, MachineOperand::CreateRegister(RAX));
          }
          break;
        }
        case MachineInstruction::Opcode::CMov: {
          auto Src = Inst->GetOperand(0);
          auto Dst = Inst->GetOperand(1);
          
          auto NewSrc = Src;
          auto NewDst = Dst;
          if(Src.IsImmediate()) {
            Inst->Parent()->InsertBefore(new MovMachineInst(Src, MachineOperand::CreateRegister(RAX)), Inst);
            NewSrc = MachineOperand::CreateRegister(RAX);
          }

          if(Dst.IsMemory()) {
            Inst->Parent()->InsertBefore(new MovMachineInst(Dst, MachineOperand::CreateRegister(RDX)), Inst);
            Inst->Parent()->InsertAfter(new MovMachineInst(MachineOperand::CreateRegister(RDX), Dst), Inst);
            NewDst = MachineOperand::CreateRegister(RDX);
          }

          Inst->ReplaceOperand(0, NewSrc);
          Inst->ReplaceOperand(1, NewDst);
          break;
        }
        default:
          break;
      }
    }
  }
  return true;
}

} // namespace klang