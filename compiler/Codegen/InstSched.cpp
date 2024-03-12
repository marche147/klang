#include <Codegen/InstSched.h>

#include <map>
#include <queue>

// https://people.cs.umass.edu/~moss/610-slides/30.pdf

namespace klang {

static void UpdateDefs(
  std::map<size_t, PrecedenceGraphNode*>& VirtDefs, 
  std::map<MachineRegister, PrecedenceGraphNode*>& PhysDefs,
  PrecedenceGraphNode* &FlagsDef, 
  PrecedenceGraphNode* Node) {
  auto *Inst = Node->Instruction();

  auto UpdateDefByOperand = [&](MachineOperand Op, PrecedenceGraphNode* Node) {
    if(Op.IsVirtualRegister()) {
      VirtDefs[Op.GetVirtualRegister()] = Node;
    } else if(Op.IsMachineRegister()) {
      PhysDefs[Op.GetRegister()] = Node;
    }
  };

  auto UpdateFlagsDef = [&](PrecedenceGraphNode* Node) {
    FlagsDef = Node;
  };

  auto InvalidateAll = [&]() {
    VirtDefs.clear();
    PhysDefs.clear();
    FlagsDef = nullptr;
  };

  switch(Inst->GetOpcode()) {
    // TODO: Add all opcodes
    case MachineInstruction::Opcode::Mov: 
    case MachineInstruction::Opcode::CMov: {
      UpdateDefByOperand(Inst->GetOperand(1), Node);
      break;
    }
    case MachineInstruction::Opcode::Add:
    case MachineInstruction::Opcode::Sub:
    case MachineInstruction::Opcode::IMul:
    case MachineInstruction::Opcode::Or:
    case MachineInstruction::Opcode::And: 
    case MachineInstruction::Opcode::Xor: {
      UpdateDefByOperand(Inst->GetOperand(1), Node);
      UpdateFlagsDef(Node);
      break;
    }

    case MachineInstruction::Opcode::Cmp: 
    case MachineInstruction::Opcode::Test: {
      UpdateFlagsDef(Node);
      break;
    }

    case MachineInstruction::Opcode::Call: {
      InvalidateAll();
      UpdateDefByOperand(MachineOperand::CreateRegister(RAX), Node);
      break;
    }

    case MachineInstruction::Opcode::Ret: 
    case MachineInstruction::Opcode::Jmp: 
    case MachineInstruction::Opcode::Jcc: {
      break;
    }

    case MachineInstruction::Opcode::Push: {
      break;
    }
    case MachineInstruction::Opcode::Pop: {
      UpdateDefByOperand(Inst->GetOperand(0), Node);
      break;
    }

    case MachineInstruction::Opcode::Lea: {
      UpdateDefByOperand(Inst->GetOperand(0), Node);
      break;
    }

    case MachineInstruction::Opcode::Cqo: {
      UpdateDefByOperand(MachineOperand::CreateRegister(RDX), Node);
      break;
    }

    case MachineInstruction::Opcode::IDiv: {
      UpdateDefByOperand(MachineOperand::CreateRegister(RAX), Node);
      UpdateDefByOperand(MachineOperand::CreateRegister(RDX), Node);
      break;
    }

    default: {
      throw std::runtime_error("Unknown opcode");
    }
  }
  return; 
}

static void AddDependency(
  PrecedenceGraphNode* Current, 
  std::map<size_t, PrecedenceGraphNode*> VirtDefs,
  std::map<MachineRegister, PrecedenceGraphNode*> PhysDefs, 
  PrecedenceGraphNode* FlagsDef) {

  auto AddDependencyByOperand = [&](MachineOperand Op) {
    if(Op.IsVirtualRegister()) {
      auto Def = VirtDefs.find(Op.GetVirtualRegister());
      if(Def != VirtDefs.end()) {
        Def->second->UsedBy(Current);
      }
    } else if(Op.IsMachineRegister()) {
      auto Def = PhysDefs.find(Op.GetRegister()); 
      if(Def != PhysDefs.end()) {
        Def->second->UsedBy(Current);
      }
    }
  };

  auto AddFlagsDependency = [&]() {
    if(FlagsDef) {
      FlagsDef->UsedBy(Current);
    }
  };

  auto *Inst = Current->Instruction();
  switch(Inst->GetOpcode()) {
    // TODO: Add all opcodes
    case MachineInstruction::Opcode::Mov: {
      AddDependencyByOperand(Inst->GetOperand(0));
      break;
    }
    case MachineInstruction::Opcode::CMov: {
      AddDependencyByOperand(Inst->GetOperand(0));
      AddFlagsDependency();
      break;
    }
    case MachineInstruction::Opcode::Add:
    case MachineInstruction::Opcode::Sub:
    case MachineInstruction::Opcode::IMul:
    case MachineInstruction::Opcode::Or:
    case MachineInstruction::Opcode::And:
    case MachineInstruction::Opcode::Xor:
    case MachineInstruction::Opcode::Cmp:
    case MachineInstruction::Opcode::Test: {
      AddDependencyByOperand(Inst->GetOperand(0));
      AddDependencyByOperand(Inst->GetOperand(1));
      break;
    }

    // barriers
    case MachineInstruction::Opcode::Call: 
    case MachineInstruction::Opcode::Ret: 
    case MachineInstruction::Opcode::Jmp:
    case MachineInstruction::Opcode::Jcc: {
      break;
    }

    case MachineInstruction::Opcode::Push: {
      AddDependencyByOperand(Inst->GetOperand(0));
    }
    case MachineInstruction::Opcode::Pop: {
      break;
    }

    case MachineInstruction::Opcode::Lea: {
      break;
    }

    case MachineInstruction::Opcode::Cqo: {
      AddDependencyByOperand(MachineOperand::CreateRegister(RAX));
      break;
    }

    case MachineInstruction::Opcode::IDiv: {
      AddDependencyByOperand(MachineOperand::CreateRegister(RAX));
      AddDependencyByOperand(MachineOperand::CreateRegister(RDX));
      AddDependencyByOperand(Inst->GetOperand(0));
      break;
    }

    default: {
      throw std::runtime_error("Unknown opcode");
    }
  }

  return;
}

static int CalculateCycle(PrecedenceGraphNode* Node) {
  auto *Inst = Node->Instruction();
  switch(Inst->GetOpcode()) {
    case MachineInstruction::Opcode::Mov:
    case MachineInstruction::Opcode::CMov: {
      // XXX: regs are not allocated yet
      // we don't know if it's a move between regs or a move between reg and mem
      return 4;
    }
    case MachineInstruction::Opcode::Or:
    case MachineInstruction::Opcode::And:
    case MachineInstruction::Opcode::Xor: 
    case MachineInstruction::Opcode::Test: {
      return 2;
    }
    case MachineInstruction::Opcode::Add:
    case MachineInstruction::Opcode::Sub:
    case MachineInstruction::Opcode::Cmp: {
      return 3;
    }

    case MachineInstruction::Opcode::IMul: {
      return 5;
    }    

    case MachineInstruction::Opcode::Call:
    case MachineInstruction::Opcode::Ret:
    case MachineInstruction::Opcode::Jmp:
    case MachineInstruction::Opcode::Jcc: {
      return 8;
    }

    case MachineInstruction::Opcode::Push:
    case MachineInstruction::Opcode::Pop: {
      return 1;
    }

    case MachineInstruction::Opcode::Lea: {
      return 1;
    }

    default: {
      throw std::runtime_error("Unknown opcode");
    }
  }
  return 0;
}

void PrecedenceGraph::Build() {
  std::map<size_t, PrecedenceGraphNode*> VirtDefs;
  std::map<MachineRegister, PrecedenceGraphNode*> PhysDefs;
  PrecedenceGraphNode* FlagsDef = nullptr;

  std::set<PrecedenceGraphNode*> BarrierNodes;

  for(auto InstIt = Block_->begin(); InstIt != Block_->end(); InstIt++) {
    auto *Inst = *InstIt;
    auto *Node = new PrecedenceGraphNode(Inst);

    if(Inst->HasSideEffects()) {
      BarrierNodes.insert(Node);
    } else {
      AddDependency(Node, VirtDefs, PhysDefs, FlagsDef); 
    }
    UpdateDefs(VirtDefs, PhysDefs, FlagsDef, Node);
    Nodes_.push_back(Node);
  }

  // handle barrier nodes
  for(auto *BarrierNode : BarrierNodes) {
    bool BeforeBarrier = true;
    for(auto *Node : Nodes_) {
      if(BeforeBarrier) {
        if(Node == BarrierNode) {
          BeforeBarrier = false;
        } else {
          Node->UsedBy(BarrierNode);
        }
      } else {
        BarrierNode->UsedBy(Node);
      }
    }
  }
}

std::vector<PrecedenceGraphNode*> PrecedenceGraph::Leaves() const {
  std::vector<PrecedenceGraphNode*> Leafs;
  for(auto *Node : Nodes_) {
    if(Node->Successors().empty()) {
      Leafs.push_back(Node);
    }
  }
  return Leafs;
}

void ListScheduler::Schedule() {
  for(auto *Block : (*Function_)) {
    ScheduleBlock(Block);
  }
}

class NodeCompare {
public:
  bool operator()(PrecedenceGraphNode* A, PrecedenceGraphNode* B) {
    return CalculateCycle(A) < CalculateCycle(B);
  }
};

void ListScheduler::ScheduleBlock(MachineBasicBlock* Block) {
  PrecedenceGraph Graph(Block);
  Graph.Build();

  int Cycle = 1;
  std::vector<PrecedenceGraphNode*> Leaves = Graph.Leaves();
  std::vector<PrecedenceGraphNode*> ActiveNodes;
  std::priority_queue<PrecedenceGraphNode*, std::vector<PrecedenceGraphNode*>, NodeCompare> ReadyNodes(Leaves.begin(), Leaves.end());
  std::map<PrecedenceGraphNode*, int> Start;
  std::vector<PrecedenceGraphNode*> Scheduled;

  while(ReadyNodes.size() > 0 || ActiveNodes.size() > 0) {    
    if(ReadyNodes.size() > 0) {
      auto *Node = ReadyNodes.top();
      ReadyNodes.pop();
      ActiveNodes.push_back(Node);
      Start[Node] = Cycle;
    }

    Cycle++;
    for(auto It = ActiveNodes.begin(); It != ActiveNodes.end(); ) {
      auto *Node = *It;
      if(Start[Node] + CalculateCycle(Node) <= Cycle) {
        // op completed
        Scheduled.push_back(Node);
        
        // add ready nodes
        for(auto *Succ : Node->Successors()) {
          if(Succ->IsReady(Scheduled)) {
            ReadyNodes.push(Succ);
          }
        }
        It = ActiveNodes.erase(It);
      } else {
        It++;
      }
    }
  }

  // change order of instructions
  for(auto *Node : Scheduled) {
    auto *Inst = Node->Instruction();
    Inst->Parent()->Remove(Inst);
  }
  for(auto *Node : Scheduled) {
    auto *Inst = Node->Instruction();
    Block->AddInstruction(Inst);
  }
}

} // namespace klang