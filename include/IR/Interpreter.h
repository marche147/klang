#ifndef _INTERPRETER_H
#define _INTERPRETER_H

#include <IR/IR.h>

#include <exception>
#include <stdexcept>
#include <functional>

namespace klang {

using InterpreterFunction = std::function<int64_t(std::vector<int64_t>&)>;

class Interpreter {
public:
  Interpreter() : Regs_() {}
  ~Interpreter();

  void AddFunction(Function* F) {
    InterpreterFunction IF = [&](std::vector<int64_t>& Args) {
      return this->Execute(F, Args);
    };
    Funcs_[F->Name()] = std::move(IF);
  }

  void AddNativeFunction(const char* Name, InterpreterFunction F) {
    Funcs_[Name] = std::move(F);
  }

  int64_t RunFunction(const char* Name, std::vector<int64_t>& Args) {
    auto It = Funcs_.find(Name);
    if(It == Funcs_.end()) {
      throw std::runtime_error("Unknown function");
    }
    return It->second(Args);
  }

private:
  int64_t Execute(Function* F, std::vector<int64_t>& Args);

  int64_t LoadOperand(Operand Op, std::vector<int64_t>& Args);
  void StoreOperand(size_t Reg, Operand Op);

  std::unordered_map<size_t, int64_t> Regs_;
  std::unordered_map<std::string, InterpreterFunction> Funcs_;
  std::vector<std::vector<int64_t>> Arrays_;
};

} // namespace klang

#endif