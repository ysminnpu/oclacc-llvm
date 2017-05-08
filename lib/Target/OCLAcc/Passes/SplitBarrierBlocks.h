//===- SplitBarrierBlocks.h - Split blocks using barriers -----------------===//
//
// Provides a pass that splits blocks with calls to OpenCL barrier functions and
// seperates those calls so that they are the last instruction before the
// terminator in a basic block or so that they are the only instruction (besides
// the terminator) in a block.
//
//===----------------------------------------------------------------------===//

#ifndef _LOOPUS_SPLITBARRIERBLOCKS_H_INCLUDE_
#define _LOOPUS_SPLITBARRIERBLOCKS_H_INCLUDE_

#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"

class SplitBarrierBlocks : public llvm::FunctionPass {
  protected:
    bool isBarrierFunctionCall(const llvm::Instruction *I) const;
    unsigned collectSplitBlocks(llvm::Function &F,
        llvm::SmallVectorImpl<llvm::BasicBlock*> &SplitBlocks);
    llvm::BasicBlock* splitOneBlock(llvm::BasicBlock* BB);
    bool splitAllBlocks(llvm::Function &F);

  public:
    static char ID;

    SplitBarrierBlocks(void);
    virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
    virtual bool runOnFunction(llvm::Function &F) override;
};

#endif

