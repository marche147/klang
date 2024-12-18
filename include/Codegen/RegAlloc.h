#ifndef _REGALLOC_H
#define _REGALLOC_H

#include <Codegen/Codegen.h>
#include <IR/Analysis.h>

namespace klang {

struct MRegLivenessState {
  MRegLivenessState() : Live_() {}

  static MRegLivenessState Empty(MachineFunction* F) {
    return MRegLivenessState();
  }

  void Meet(const MRegLivenessState& Other);
  void Transfer(const MachineInstruction* Inst);

  void Print() const;

  bool operator==(const MRegLivenessState& Other) const {
    return Live_ == Other.Live_;
  }

  bool Contains(size_t Reg) const {
    return Live_.count(Reg);
  }

  std::set<size_t> Live_;
};

template<typename T, bool Direction = true>
AnalysisResult<T, MachineBasicBlock> MFDataflowAnalysis(MachineFunction* F) {
  return DoAnalysis<T, MachineBasicBlock, MachineFunction, Direction>(F);
}

class Interval {
public:
  Interval(size_t VirtRegId, int Start, int End) : VirtRegId_(VirtRegId), Start_(Start), End_(End), SpillAt_(-1), SpillSlot_(0), Reg_(None) {}

  int Start() const { return Start_; }
  int End() const { return End_; }
  size_t VirtRegId() const { return VirtRegId_; }

  bool IsSpilled() const { return SpillAt_ != -1; }
  int SpillAt() const { return SpillAt_; }
  int SpillSlot() const { return SpillSlot_; }

  void SetReg(MachineRegister Reg) { Reg_ = Reg; }
  MachineRegister Reg() const { return Reg_; }

  void SpillAt(int Time, int Slot) {
    SpillAt_ = Time;
    SpillSlot_ = Slot;
  
  }

private:
  size_t VirtRegId_;
  int SpillAt_, SpillSlot_;
  MachineRegister Reg_;
  int Start_, End_;
};

class LinearScanRegAlloc {
public:
  LinearScanRegAlloc(MachineFunction* Func) : Func_(Func) {}

  bool Allocate();

  void EmitPrologue();
  void EmitEpilogue();

private:
  std::vector<MachineBasicBlock*> SortBlocks();
  std::vector<Interval*> ComputeInterval(const std::vector<MachineBasicBlock*>& Blocks);
  void ComputeIntervalSingle(const std::vector<MachineBasicBlock*>& Blocks, MachineInstruction* Inst, std::vector<Interval*>& Intervals);
  void FixupCallInst(MachineInstruction* Inst, std::vector<Interval*>& Intervals);

  bool InstructionInLoop(MachineInstruction* Inst);
  std::pair<MachineBasicBlock*, MachineBasicBlock*> FindLoopEntryExitBlock(MachineBasicBlock* Block);
  
  int AllocateSpillSlot(Interval* I) {
    if(SpilledIntervals_.count(I))
      return SpilledIntervals_[I];
    int Slot = SpilledIntervals_.size();
    SpilledIntervals_[I] = Slot;
    return Slot;
  }

  void DumpOrderedInstructions() {
    std::stringstream SS;
    for(size_t i = 0; i < OrderToInst_.size(); i++) {
      SS.str("");
      OrderToInst_[i]->Emit(SS);
      std::cerr << i << ": " << SS.str() << std::endl;
    }
  }

  MachineFunction* Func_;
  std::unordered_map<MachineInstruction*, int> InstToOrder_;
  std::unordered_map<int, MachineInstruction*> OrderToInst_;
  std::unordered_map<size_t, Interval*> VirtRegToInterval_;
  std::unordered_map<Interval*, int> SpilledIntervals_;

  State<MRegLivenessState, MachineBasicBlock> LiveIn_, LiveOut_;
};

bool FixupInstruction(MachineFunction* F);

} // namespace klang

#endif