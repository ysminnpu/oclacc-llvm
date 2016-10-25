//===- InstSimplify.h -----------------------------------------------------===//
//
// Tries to replace certain instructions by ones easier/faster to implement om
// hardware devices.
//
//===----------------------------------------------------------------------===//

#ifndef _LOOPUS_INSTSIMPLIFY_H_INCLUDE_
#define _LOOPUS_INSTSIMPLIFY_H_INCLUDE_

#include "llvm/IR/Function.h"
#include "llvm/Pass.h"

class InstSimplify : public llvm::FunctionPass {
  protected:
    bool handleInst(llvm::Instruction *I);

  public:
    static char ID;

    InstSimplify(void);
    virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
    virtual bool runOnFunction(llvm::Function &F) override;
};

#endif

