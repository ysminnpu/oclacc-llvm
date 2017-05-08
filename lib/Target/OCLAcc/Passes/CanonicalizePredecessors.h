//===- CanonicalizePredecessors.h -----------------------------------------===//
//
// Canonicalize certain patterns in a way that the contained basic blocks do
// only have two predecessors.
//
//===----------------------------------------------------------------------===//

#ifndef _LOOPUS_CANONICALIZEPREDECESSORS_H_INCLUDE_
#define _LOOPUS_CANONICALIZEPREDECESSORS_H_INCLUDE_

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"

namespace Loopus {
  struct InjectPattern {
    llvm::BasicBlock *HeadBB;
    struct {
      llvm::BasicBlock *BB;
      unsigned NumSucc;
    } LeftPred, RightPred;
    llvm::BasicBlock *TailBB;

    InjectPattern(void)
     : HeadBB(nullptr), TailBB(nullptr) {
      LeftPred.BB = nullptr; LeftPred.NumSucc = 0;
      RightPred.BB = nullptr; RightPred.NumSucc = 0;
    }
  };

  class CanonicalizePredecessorsWorker {
    private:
      llvm::DominatorTree *DT;
      llvm::LoopInfo *LI;

    protected:
      bool isInjectPatternHead(llvm::BasicBlock *BB, struct InjectPattern *Pattern);
      void updateDomTree(struct InjectPattern *Pattern, llvm::BasicBlock *NewTail);
      void updateLoopInfo(struct InjectPattern *Pattern, llvm::BasicBlock *NewTail);
      bool injectDedicatedTail(struct InjectPattern *Pattern);

    public:
      CanonicalizePredecessorsWorker(llvm::DominatorTree *DT,
          llvm::LoopInfo *LI);

      bool runOnBasicBlock(llvm::BasicBlock *BB);
      bool runOnFunction(llvm::Function *F);
  };
}

class CanonicalizePredecessors : public llvm::FunctionPass {
  public:
    static char ID;

    CanonicalizePredecessors(void);
    virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
    virtual bool runOnFunction(llvm::Function &F) override;
};

#endif
