//===- DelayStores.h - Try to delay stores --------------------------------===//
//
// Tries to delay stores out of if-then-else patterns by moving the stores from
// the true and false branch into the tail block if they are storing into the
// same memory location.
//
//===----------------------------------------------------------------------===//

#ifndef _LOOPUS_DELAYSTORES_H_INCLUDE_
#define _LOOPUS_DELAYSTORES_H_INCLUDE_

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"

namespace Loopus {
  /// \brief A little helper struct that stores all needed blocks and some
  /// \brief additional information.
  struct DelayPattern {
    enum DelayPatternKind {
      Undefined = 0, Triangle, Diamond
    };

    // The blocks belonging to the processed pattern. If the pattern is a
    // triangle the head block only has one real branch. That branch block
    // should always be stored in BranchBB0 to make life easier ;-)
    // no matter if it is reached if the corresponding branch uses it in the
    // then- or else-part.
    llvm::BasicBlock *HeadBB, *BranchBB0, *BranchBB1, *TailBB;
    DelayPatternKind Kind;
    // Indicates if the tail block has other predecessors than the branch blocks.
    bool TailHasOtherPreds;

    DelayPattern(void)
      : HeadBB(nullptr), BranchBB0(nullptr), BranchBB1(nullptr), TailBB(nullptr),
      Kind(DelayPatternKind::Undefined), TailHasOtherPreds(false) {
      }
    DelayPattern(llvm::BasicBlock *Head)
      : HeadBB(Head), BranchBB0(nullptr), BranchBB1(nullptr), TailBB(nullptr),
      Kind(DelayPatternKind::Undefined), TailHasOtherPreds(false) {
      }
  };

  class DelayStoresWorker {
    private:
      llvm::AliasAnalysis *AA;

    protected:
      bool isPatternHead(DelayPattern &pattern);
      bool canInstructionRangeModRefHW(const llvm::Instruction &Start,
          const llvm::Instruction &End, const llvm::AliasAnalysis::Location &Loc,
          const llvm::AliasAnalysis::ModRefResult Mod);
      bool isLoadHoistBarrierInRange(const llvm::Instruction &Start,
          const llvm::Instruction &End, llvm::LoadInst *LI);
      bool isStoreSinkBarrierInRange(const llvm::Instruction &Start,
          const llvm::Instruction &End, llvm::AliasAnalysis::Location &LI);
      bool isSafeToHoist(const llvm::Instruction *I) const;

      llvm::Instruction* hoistInstruction(llvm::BasicBlock *TBB, llvm::Instruction *SRCI);
      llvm::LoadInst* canHoistFromBlock(llvm::BasicBlock *BB, llvm::LoadInst *LI);
      bool hoistLoad(llvm::BasicBlock *HeadBB, llvm::LoadInst *LD0, llvm::LoadInst *LD1);
      bool mergeLoads(Loopus::DelayPattern &pattern);

      llvm::StoreInst* canSinkFromBlock(llvm::BasicBlock *BB1, llvm::StoreInst *ST0);
      bool sinkStore(llvm::BasicBlock *TailBB, llvm::StoreInst *ST0, llvm::StoreInst *ST1);
      bool mergeStores(Loopus::DelayPattern &pattern);

    public:
      DelayStoresWorker(llvm::AliasAnalysis *AA);

      bool runOnBasicBlock(llvm::BasicBlock *BB);
      bool runOnFunction(llvm::Function *F);

  };
}

class DelayStores : public llvm::FunctionPass {
  public:
    static char ID;

    DelayStores(void);
    virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
    virtual bool runOnFunction(llvm::Function &F) override;
};

#endif

