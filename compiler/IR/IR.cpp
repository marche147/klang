#include <IR/IR.h>

#include <iostream>
#include <algorithm>

namespace klang {

Function::~Function() {
  for(auto BB : BasicBlocks_) {
    delete BB;
  }
}

BasicBlock::~BasicBlock() {
  auto *Inst = Head_;
  while(Inst != nullptr) {
    auto *Next = Inst->Next();
    delete Inst;
    Inst = Next;
  }
}

Instruction::~Instruction() {
}

void BasicBlock::Iterator::UpdateLinks() {
  if(Current_ == nullptr) {
    Next_ = nullptr;
    Prev_ = nullptr;
    return;
  }
  Next_ = Current_->Next();
  Prev_ = Current_->Prev();
}

BasicBlock::Iterator::Iterator(Instruction* Inst, bool Reverse) : Current_(Inst), Reverse_(Reverse) {
  UpdateLinks();
}

BasicBlock::Iterator& BasicBlock::Iterator::operator++() {
  if(Reverse_)
    Current_ = Prev_;
  else
    Current_ = Next_;
  UpdateLinks();
  return *this;
}

BasicBlock::Iterator BasicBlock::Iterator::operator++(int) {
  auto Ret = *this;
  if(Reverse_)
    Current_ = Prev_;
  else
    Current_ = Next_;
  UpdateLinks();
  return Ret;
}

void BasicBlock::Print() const {
  std::cout << "bb" << Index() << ":\n";
  auto *Cur = Head_;
  while(Cur != nullptr) {
    std::cout << "\t";
    Cur->Print();
    std::cout << "\n";
    Cur = Cur->Next();
  }
  return;
}

void Function::Print() const {
  std::cout << "define " << Name_ << "\n";
  for(auto *BB : BasicBlocks_) {
    BB->Print();
    std::cout << "\n";
  }
}

void Module::AddFunction(Function* Func) {
  assert(Func->Parent() == nullptr && "Function already belongs to a module");
  Functions_.push_back(Func);
  Func->SetParent(this);
}

void Function::AddBasicBlock(BasicBlock* BB) {
  assert(BB->Parent() == nullptr && "Basic block already belongs to a function");
  BasicBlocks_.push_back(BB);
  BB->SetParent(this);
  BB->SetIndex(BasicBlocks_.size());
}

BasicBlock* Function::Remove(BasicBlock* BB) {
  auto It = std::find(BasicBlocks_.begin(), BasicBlocks_.end(), BB);
  if(It != BasicBlocks_.end()) {
    BasicBlocks_.erase(It);
    BB->SetParent(nullptr);
    return BB;
  }
  return nullptr;
}

void BasicBlock::AddInstruction(Instruction* Inst) {
  assert(Inst->Parent() == nullptr && "Instruction already belongs to a basic block");
  if(Head_ == nullptr) {
    Head_ = Inst;
  }
  Inst->SetPrev(Tail_);
  if(Tail_ != nullptr) {
    Tail_->SetNext(Inst);
  }
  Tail_ = Inst;
  
  Inst->SetParent(this);
  Size_++;
}

void BasicBlock::InsertAfter(Instruction* Inst, Instruction* After) {
  assert(After->Parent() == this && "Instruction does not belong to this basic block");
  assert(Inst->Parent() == nullptr && "Instruction already belongs to a basic block");
  if(After == Tail_) {
    Inst->SetPrev(Tail_);
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

void BasicBlock::InsertBefore(Instruction* Inst, Instruction* Before) {
  assert(Before->Parent() == this && "Instruction does not belong to this basic block");
  assert(Inst->Parent() == nullptr && "Instruction already belongs to a basic block");
  if(Before == Head_) {
    Inst->SetNext(Head_);
    Head_->SetPrev(Inst);
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

Instruction* BasicBlock::Replace(Instruction* Inst, Instruction* Replaced) {
  assert(Inst->Parent() == nullptr && "Instruction already belongs to a basic block");
  assert(Replaced->Parent() == this && "Instruction does not belong to this basic block");

  if(Replaced == Head_) {
    Head_ = Inst;
  }
  if(Replaced == Tail_) {
    Tail_ = Inst;
  }
  
  auto *Next = Replaced->Next();
  if(Next) {
    Next->SetPrev(Inst);
  }
  Inst->SetNext(Next);
  
  auto *Prev = Replaced->Prev();
  if(Prev) {
    Prev->SetNext(Inst);
  }
  Inst->SetPrev(Prev);
  Inst->SetParent(this);
  
  Replaced->SetParent(nullptr);
  Replaced->SetNext(nullptr);
  Replaced->SetPrev(nullptr);
  return Replaced;
}

Instruction* BasicBlock::Remove(Instruction* Inst) {
  assert(Inst->Parent() == this && "Instruction does not belong to this basic block");
  if(Inst == Head_) {
    Head_ = Inst->Next();
  }
  if(Inst == Tail_) {
    Tail_ = Inst->Prev();
  }
  if(Inst->Next()) {
    Inst->Next()->SetPrev(Inst->Prev());
  }
  if(Inst->Prev()) {
    Inst->Prev()->SetNext(Inst->Next());
  }
  Inst->SetParent(nullptr);
  Inst->SetNext(nullptr);
  Inst->SetPrev(nullptr);
  Size_--;
  return Inst;
}

Operand Instruction::GetOperand(size_t Id) const { 
  assert(Id < Operands_.size() && "Invalid operand id");
  return Operands_[Id]; 
}

void Instruction::SetOperand(size_t Id, const Operand& Op) { 
  assert(Id < Operands_.size() && "Invalid operand id");
  Operands_[Id] = Op; 
}

std::vector<BasicBlock*> Function::PostOrder() const {
  std::vector<BasicBlock*> PostOrder;
  std::set<BasicBlock*> Visited;
  PostOrderImpl(BasicBlocks_[0], Visited, PostOrder);
  return PostOrder;
}

void Function::PostOrderImpl(BasicBlock* Current, std::set<BasicBlock*> Visited, std::vector<BasicBlock*>& PostOrder) const {
  if(Visited.count(Current)) {
    return;
  }
  Visited.insert(Current);
  for(auto Succ : Current->Successors()) {
    PostOrderImpl(Succ, Visited, PostOrder);
  }
  PostOrder.push_back(Current);
}

std::vector<BasicBlock*> BasicBlock::Successors() const {
  std::vector<BasicBlock*> Successors;
  auto *Inst = Tail_;
  assert(Inst->IsTerminator() && "Basic block does not end with a terminator instruction");
  for(size_t i = 0; i < Inst->NumSuccessor(); ++i) {
    Successors.push_back(Inst->Successor(i));
  }
  return Successors;
}

std::vector<BasicBlock*> BasicBlock::Predecessors() const {
  std::vector<BasicBlock*> Predecessors;
  for(auto *BB : (*Parent_)) {
    for(auto *Succ : BB->Successors()) {
      if(Succ == this) {
        Predecessors.push_back(BB);
      }
    }
  }
  return Predecessors;
}

bool BasicBlock::IsExit() const {
  assert(Tail_ && "Basic block has no terminator");
  return Tail_->Type() == Instruction::Ret || Tail_->Type() == Instruction::RetVoid; 
}

void Operand::Print() const {
  if(Type_ == Register) {
    std::cout << "%" << U_.RegId;
  } else if(Type_ == Immediate) {
    std::cout << "#" << std::hex << U_.Imm << std::dec;
  } else {
    std::cout << "$" << U_.Param;
  }
}

void NopInst::Print() const {
  std::cout << "nop";
}

void AssignInst::Print() const {
  GetOperand(0).Print();
  std::cout << " = ";
  GetOperand(1).Print();
}

void BinaryInst::Print() const {
  const char* Op;
  switch(Operation_) {
    case Add: Op = "+"; break;
    case Sub: Op = "-"; break;
    case Mul: Op = "*"; break;
    case Div: Op = "/"; break;
    case Mod: Op = "%"; break;
    case And: Op = "&"; break;
    case Or: Op = "|"; break;
    case Xor: Op = "^"; break;
    case Shl: Op = "<<"; break;
    case Shr: Op = ">>"; break;
    case Lt: Op = "<"; break;
    case Le: Op = "<="; break;
    case Gt: Op = ">"; break;
    case Ge: Op = ">="; break;
    case Eq: Op = "=="; break;
    case Ne: Op = "!="; break;
    default: assert(false && "Invalid binary operation");
  }

  GetOperand(0).Print();
  std::cout << " = ";
  GetOperand(1).Print();
  std::cout << " " << Op << " ";
  GetOperand(2).Print();
}

void JmpInst::Print() const {
  std::cout << "jmp bb" << Successors_[0]->Index();;
}

void JnzInst::Print() const {
  std::cout << "jnz ";
  GetOperand(0).Print();
  std::cout << ", bb" << Successors_[0]->Index() << ", bb" << Successors_[1]->Index();
}

void RetInst::Print() const {
  std::cout << "ret ";
  GetOperand(0).Print();
}

void RetVoidInst::Print() const {
  std::cout << "ret void";
}

void CallInst::Print() const {
  GetOut(0).Print();
  std::cout << " = call " << Callee_;
  for(size_t i = 0; i < Ins(); i++) {
    std::cout << " ";
    GetIn(i).Print();
  }
}

void CallVoidInst::Print() const {
  std::cout << "call " << Callee_;
  for(size_t i = 0; i < Ins(); i++) {
    std::cout << " ";
    GetIn(i).Print();
  }
}

void ArrayNewInst::Print() const {
  GetOut(0).Print();
  std::cout << " = array_new ";
  GetIn(0).Print();
}

void ArrayLoadInst::Print() const {
  GetOut(0).Print();
  std::cout << " = ";
  GetIn(0).Print();
  std::cout << "[";
  GetIn(1).Print();
  std::cout << "]";
}

void ArrayStoreInst::Print() const {
  GetIn(0).Print();
  std::cout << "[";
  GetIn(1).Print();
  std::cout << "] = ";
  GetIn(2).Print();
}

void LoadLabelInst::Print() const {
  GetOut(0).Print();
  std::cout << " = load_label " << Label_;
}

int64_t BinaryInst::Evaluate(Operation Op, int64_t Op1, int64_t Op2) {
  int64_t Result = 0;
  switch(Op) {
    case Add: Result = Op1 + Op2; break;
    case Sub: Result = Op1 - Op2; break;
    case Mul: Result = Op1 * Op2; break;
    case Div: Result = Op1 / Op2; break;
    case Mod: Result = Op1 % Op2; break;
    case And: Result = Op1 & Op2; break;
    case Or: Result = Op1 | Op2; break;
    case Xor: Result = Op1 ^ Op2; break;
    case Shl: Result = Op1 << Op2; break;
    case Shr: Result = Op1 >> Op2; break;
    case Lt: {
      Result = Op1 < Op2 ? 1 : 0;
      break;
    }
    case Le: {
      Result = Op1 <= Op2 ? 1 : 0;
      break;
    }
    case Gt: {
      Result = Op1 > Op2 ? 1 : 0;
      break;
    }
    case Ge: {
      Result = Op1 >= Op2 ? 1 : 0;
      break;
    }
    case Eq: {
      Result = Op1 == Op2 ? 1 : 0;
      break;
    }
    case Ne: {
      Result = Op1 != Op2 ? 1 : 0;
      break;
    }
    default: assert(false && "Invalid binary operation");
  }
  return Result;
}

void FuncBuilder::Emit(Instruction* Inst) {
  assert(CurrentBlock_ && "No current basic block");
  CurrentBlock_->AddInstruction(Inst);
}

BasicBlock* FuncBuilder::CreateBlock() {
  auto *Block = new BasicBlock();
  Function_->AddBasicBlock(Block);
  return Block;
}

} // namespace klang