#ifndef _ANALYSIS_H
#define _ANALYSIS_H

#include <IR/IR.h>

#include <unordered_map>
#include <algorithm>
#include <deque>
#include <set>

namespace klang {

template <typename T, typename BB>
using State = std::unordered_map<BB*, T>;

template <typename T, typename BB>
using AnalysisResult = std::pair<State<T, BB>, State<T, BB>>;

template <typename T>
class WorkList {
public:
  WorkList() = default;

  template <typename IterTy>
  WorkList(IterTy Begin, IterTy End) : Queue_(Begin, End), Entries_() {
    Entries_.insert(Begin, End);
  }

  bool Empty() const { return Queue_.empty(); }
  bool Contains(const T& Entry) const { return Entries_.count(Entry); }

  void Add(const T& Entry) {
    if(!Contains(Entry)) {
      Entries_.insert(Entry);
      Queue_.push_back(Entry);
    }
  }

  T Pop() {
    T ret = Queue_.front();
    Queue_.pop_front();
    Entries_.erase(ret);
    return ret;
  }

private: 
  std::set<T> Entries_;
  std::deque<T> Queue_;
};

template<typename BB>
using BasicBlockWorkList = WorkList<BB*>;

template <typename T, typename BBT, typename FNT, bool Direction = true>
AnalysisResult<T, BBT> DoAnalysis(FNT* F) {
  auto BBs = F->PostOrder();
  if(Direction) {
    std::reverse(BBs.begin(), BBs.end());
  }
  BasicBlockWorkList<BBT> WorkList(BBs.begin(), BBs.end());

  State<T, BBT> In, Out;

  while(!WorkList.Empty()) {
    auto *BB = WorkList.Pop();

    T InState = T::Empty(F);
    if(Direction) {
      for(auto *Pred : BB->Predecessors()) {
        InState.Meet(Out[Pred]);
      }
    } else {
      for(auto *Succ : BB->Successors()) {
        InState.Meet(In[Succ]);
      }
    }

    if(Direction) {
      if(In.count(BB) != 0 && In[BB] == InState) {
        continue;      
      }
      In[BB] = InState;
    } else {
      if(Out.count(BB) != 0 && Out[BB] == InState) {
        continue; 
      }
      Out[BB] = InState;
    }

    if(Direction) {
      for(auto It = BB->begin(); It != BB->end(); ++It) {
        InState.Transfer(*It);
      }
    } else {
      for(auto It = BB->rbegin(); It != BB->rend(); ++It) {
        InState.Transfer(*It);
      }
    }

    if(Direction) {
      if(Out.count(BB) != 0 && Out[BB] == InState) {
        continue; 
      }
    } else {
      if(In.count(BB) != 0 && In[BB] == InState) {
        continue; 
      }
    }

    if(Direction) {
      for(auto *Succ : BB->Successors()) {
        WorkList.Add(Succ);
      }
    } else {
      for(auto *Pred : BB->Predecessors()) {
        WorkList.Add(Pred);
      }
    }

    if(Direction) {
      Out[BB] = InState;
    } else {
      In[BB] = InState;
    }
  }
  return std::make_pair(In, Out);
}

template<typename T, bool Direction = true>
AnalysisResult<T, BasicBlock> DataflowAnalysis(Function* F) {
  return DoAnalysis<T, BasicBlock, Function, Direction>(F);
}

} // namespace klang

#endif 