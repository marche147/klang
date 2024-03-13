#ifndef _OPTIMIZE_H
#define _OPTIMIZE_H

#include <IR/IR.h>

#include <map>

#define DEBUG_OPTIMIZE 1

namespace klang {

#pragma region ConstPropagate
constexpr int kConstPropUndet = 0;
constexpr int kConstPropConstant = 1;
constexpr int kConstPropNonConstant = 2;

struct ConstPropValue {
  int State_;
  int64_t Value_;

  ConstPropValue() : State_(kConstPropUndet), Value_(0) {}
  ConstPropValue(int State, int64_t Value) : State_(State), Value_(Value) {}

  void SetNonConstant() {
    State_ = kConstPropNonConstant;
    Value_ = 0;
  }

  void SetConstant(int64_t Value) {
    State_ = kConstPropConstant;
    Value_ = Value;
  }

  void SetUndet() {
    State_ = kConstPropUndet;
    Value_ = 0;
  }

  bool operator==(const ConstPropValue& Other) const {
    return State_ == Other.State_ && Value_ == Other.Value_;
  }
};

struct ConstPropState {
  ConstPropState() : State_() {}

  static ConstPropState Empty(Function* Func) {
    return ConstPropState();
  }
  void Meet(const ConstPropState& Other);
  void Transfer(const Instruction& Inst);
  bool operator==(const ConstPropState& Other) const {
    return State_ == Other.State_;
  } 

  bool IsConstant(size_t Reg) const {
    auto It = State_.find(Reg);
    if(It == State_.end()) {
      return false;
    }
    return It->second.State_ == kConstPropConstant;
  }

  int64_t GetConstant(size_t Reg) const {
    auto It = State_.find(Reg);
    if(It == State_.end()) {
      return 0;
    }
    return It->second.Value_;
  }

  ConstPropValue FromOperand(const Operand& Op) const {
    if(Op.IsImmediate()) {
      return ConstPropValue(kConstPropConstant, Op.Imm());
    }
    if(Op.IsRegister()) {
      if(State_.count(Op.RegId()) == 0) {
        return ConstPropValue(kConstPropUndet, 0);
      }
      return State_.at(Op.RegId());
    }
    return ConstPropValue(kConstPropNonConstant, 0);
  }

  ConstPropValue Get(size_t Reg) const {
    auto It = State_.find(Reg);
    if(It == State_.end()) {
      return ConstPropValue(kConstPropUndet, 0);
    }
    return It->second;
  }

  std::map<size_t, ConstPropValue> State_; 
};

bool ConstantPropagate(Function* F);
#pragma endregion

#pragma region CopyPropagate
struct CopyPropState {
  CopyPropState() : State_() {}

  static CopyPropState Empty(Function* Func) {
    return CopyPropState();
  }

  void Meet(const CopyPropState& Other);
  void Transfer(const Instruction& Inst);

  bool operator==(const CopyPropState& Other) const {
    return State_ == Other.State_;
  }

  std::optional<size_t> Get(size_t Reg) const {
    auto It = State_.find(Reg);
    if(It == State_.end()) {
      return std::nullopt;
    }
    return It->second;
  }

  std::map<size_t, size_t> State_;
};

bool CopyPropagate(Function* F);
#pragma endregion

#pragma region CommonSubexpressionElimination
struct CSEValue {
  CSEValue() : Operation_(BinaryInst::Add), RHS1(), RHS2() {}
  CSEValue(BinaryInst::Operation Op, Operand RHS1, Operand RHS2) : Operation_(Op), RHS1(RHS1), RHS2(RHS2) {}

  bool operator==(const CSEValue& Other) const {
    return Operation_ == Other.Operation_ && RHS1 == Other.RHS1 && RHS2 == Other.RHS2;
  }

  bool operator<(const CSEValue& Other) const {
    if(Operation_ != Other.Operation_) {
      return Operation_ < Other.Operation_;
    }
    if(RHS1 != Other.RHS1) {
      return RHS1 < Other.RHS1;
    }
    return RHS2 < Other.RHS2;
  }

  static std::optional<CSEValue> FromInstruction(const Instruction& Inst);

  BinaryInst::Operation Operation_;
  Operand RHS1, RHS2;
};

struct GCSEState {
  GCSEState() : Values_(), Init_(false) {}

  static GCSEState Empty(Function* Func) {
    return GCSEState();
  }

  void Meet(const GCSEState& Other);
  void Transfer(const Instruction& Inst);

  bool operator==(const GCSEState& Other) const {
    return Values_ == Other.Values_ && Init_ == Other.Init_;
  }

  void Intersect(const GCSEState& Other);
  bool Contains(const CSEValue& Value) const;

  std::set<CSEValue> Values_;
  bool Init_;
};

bool LocalCSE(Function* F);
bool GlobalCSE(Function* F);
#pragma endregion

#pragma region DeadCodeElimination
struct LivenessState {
  LivenessState() : LiveRegs_() {}

  static LivenessState Empty(Function* Func) {
    return LivenessState();
  }

  void Meet(const LivenessState& Other);
  void Transfer(const Instruction& Inst);

  bool operator==(const LivenessState& Other) const {
    return LiveRegs_ == Other.LiveRegs_;
  }

  bool Contains(size_t Reg) const {
    return LiveRegs_.count(Reg) > 0;
  }

  std::set<size_t> LiveRegs_;
};

bool DeadCodeElimination(Function* F);
#pragma endregion

void OptimizeIR(Function* F);

} // namespace klang

#endif 