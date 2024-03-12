#include <IR/Interpreter.h>

#include <exception>

namespace klang {

Interpreter::~Interpreter() {
}

int64_t Interpreter::LoadOperand(Operand Op, std::vector<int64_t>& Args) {
  if(Op.IsImmediate()) {
    return Op.Imm();
  } else if(Op.IsRegister()) {
    auto It = Regs_.find(Op.RegId());
    if(It == Regs_.end()) {
      throw std::runtime_error("Unknown register");
    }
    return It->second;
  } else if(Op.IsParameter()) {
    return Args[Op.Param()];
  }
  throw std::runtime_error("Unknown operand type");
}

void Interpreter::StoreOperand(size_t Reg, Operand Op) {
  if(Op.IsImmediate()) {
    Regs_[Reg] = Op.Imm();
  } else if(Op.IsRegister()) {
    auto It = Regs_.find(Op.RegId());
    if(It == Regs_.end()) {
      throw std::runtime_error("Unknown register");
    }
    Regs_[Reg] = It->second;
  } else {
    throw std::runtime_error("Unknown operand type");
  }
}

int64_t Interpreter::Execute(Function* F, std::vector<int64_t>& Args) {
  auto* Current = F->Entry();
  while(true) {
L:
    auto* Inst = Current->Head();
    while(Inst != nullptr) {
      // XXX: Add instructions
      auto Type = Inst->Type();
      switch(Type) {
        case Instruction::Nop: {
          break;
        }
        case Instruction::Assign: {
          auto In = LoadOperand(Inst->GetIn(0), Args);
          StoreOperand(Inst->GetOut(0).RegId(), Operand::CreateImmediate(In));
          break;
        }
        case Instruction::Binary: {
          auto *B = static_cast<BinaryInst*>(Inst);
          auto RHS1 = LoadOperand(Inst->GetIn(0), Args);
          auto RHS2 = LoadOperand(Inst->GetIn(1), Args);
          auto Result = BinaryInst::Evaluate(B->GetOperation(), RHS1, RHS2);
          StoreOperand(Inst->GetOut(0).RegId(), Operand::CreateImmediate(Result));
          break;
        }

        // CF related instructions
        case Instruction::Ret: {
          return LoadOperand(Inst->GetIn(0), Args);
        }
        case Instruction::RetVoid: {
          return 0;   // Not used
        }
        case Instruction::Jmp: {
          auto *J = static_cast<JmpInst*>(Inst);
          Current = J->Successor(0);
          goto L;
        }
        case Instruction::Jnz: {
          auto *J = static_cast<JnzInst*>(Inst);
          auto Cond = LoadOperand(J->GetIn(0), Args);
          if(Cond != 0) {
            Current = J->Successor(0);
            goto L;
          } else {
            Current = J->Successor(1);
            goto L;
          }
        }
        
        // calls
        case Instruction::Call: {
          auto *C = static_cast<CallInst*>(Inst);
          std::vector<int64_t> CallArgs;
          for(size_t i = 0; i < C->Ins(); ++i) {
            CallArgs.push_back(LoadOperand(C->GetIn(i), Args));
          }
          auto Result = RunFunction(C->Callee(), CallArgs);
          StoreOperand(C->GetOut(0).RegId(), Operand::CreateImmediate(Result));
          break;
        }
        case Instruction::CallVoid: {
          auto *C = static_cast<CallVoidInst*>(Inst);
          std::vector<int64_t> CallArgs;
          for(size_t i = 0; i < C->Ins(); ++i) {
            CallArgs.push_back(LoadOperand(C->GetIn(i), Args));
          }
          RunFunction(C->Callee(), CallArgs);
          break;
        }

        // Arrays
        case Instruction::ArrayNew: {
          auto Length = LoadOperand(Inst->GetIn(0), Args);
          auto NewIndex = static_cast<int64_t>(Arrays_.size());
          Arrays_.push_back(std::vector<int64_t>(Length, 0));
          StoreOperand(Inst->GetOut(0).RegId(), Operand::CreateImmediate(NewIndex));
          break;
        }
        case Instruction::ArrayLoad: {
          auto Index = LoadOperand(Inst->GetIn(1), Args);
          auto Array = LoadOperand(Inst->GetIn(0), Args);
          if(Arrays_.size() <= Array) {
            throw std::runtime_error("No such array");
          }
          if(Index >= Arrays_[Array].size()) {
            throw std::runtime_error("Index out of bounds");
          }
          auto Value = Arrays_[Array][Index];
          StoreOperand(Inst->GetOut(0).RegId(), Operand::CreateImmediate(Value));
          break;
        }
        case Instruction::ArrayStore: {
          auto Index = LoadOperand(Inst->GetIn(1), Args);
          auto Array = LoadOperand(Inst->GetIn(0), Args);
          if(Arrays_.size() <= Array) {
            throw std::runtime_error("No such array");
          }
          if(Index >= Arrays_[Array].size()) {
            throw std::runtime_error("Index out of bounds");
          }
          auto Value = LoadOperand(Inst->GetIn(2), Args);
          Arrays_[Array][Index] = Value;
          break;
        }

        default: {
          throw std::runtime_error("Unknown instruction type");
        }
      }
    }
  }
}

} // namespace klang