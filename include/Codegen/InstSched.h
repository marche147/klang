#ifndef _INSTSCHED_H
#define _INSTSCHED_H

#include <Codegen/Codegen.h>

#define DEBUG_INSTSCHED 0

namespace klang {

class PrecedenceGraphNode {
public:
  PrecedenceGraphNode(MachineInstruction* Inst) : Inst_(Inst), Succs_(), Preds_() {}

  MachineInstruction* Instruction() const { return Inst_; }

  const std::set<PrecedenceGraphNode*>& Successors() const { return Succs_; }
  const std::set<PrecedenceGraphNode*>& Predecessors() const { return Preds_; }

  bool IsReady(const std::vector<PrecedenceGraphNode*>& Scheduled) const {
    for(auto *Pred : Preds_) {
      if(std::find(Scheduled.begin(), Scheduled.end(), Pred) == Scheduled.end()) {
        return false;
      }
    }
    return true;
  }

  void UsedBy(PrecedenceGraphNode* Node) { 
    AddSuccessor(Node); 
    Node->AddPredecessor(this);
  }

protected:
  void AddSuccessor(PrecedenceGraphNode* Node) { Succs_.insert(Node); }
  void AddPredecessor(PrecedenceGraphNode* Node) { Preds_.insert(Node); }

private:
  MachineInstruction* Inst_;
  std::set<PrecedenceGraphNode*> Succs_;
  std::set<PrecedenceGraphNode*> Preds_;
};

class PrecedenceGraph {
public:
  PrecedenceGraph(MachineBasicBlock* Block) : Block_(Block) {}
  ~PrecedenceGraph() {
    for(auto *Node : Nodes_) {
      delete Node;
    }
  }

  void Build();

  size_t Size() const { return Nodes_.size(); }
  const std::vector<PrecedenceGraphNode*>& Nodes() const { return Nodes_; }

  std::vector<PrecedenceGraphNode*> Leaves() const;

private:

  MachineBasicBlock* Block_;
  std::vector<PrecedenceGraphNode*> Nodes_;
};

class ListScheduler {
public:
  ListScheduler(MachineFunction* Function) : Function_(Function) {}

  void Schedule(); 

private:
  void ScheduleBlock(MachineBasicBlock* Block);

  MachineFunction* Function_;
};

} // namespace klang

#endif 