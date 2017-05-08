//===- CFGOptimizer.h -----------------------------------------------------===//
//
// Optimizes the control flow graph by executing several optimizations as often
// as possible.
//
//===----------------------------------------------------------------------===//

#ifndef _LOOPUS_CFGOPTIMIZER_H_INCLUDE_
#define _LOOPUS_CFGOPTIMIZER_H_INCLUDE_

#include "llvm/IR/Function.h"
#include "llvm/Pass.h"

class CFGOptimizer : public llvm::FunctionPass {
  public:
    static char ID;

    CFGOptimizer(void);
    virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
    virtual bool runOnFunction(llvm::Function &F) override;
};

#endif
