//===- SimplifyIDCalls.h --------------------------------------------------===//
//
// Replace some ID-related calls to builtin function by a computation.
//
//===----------------------------------------------------------------------===//

#ifndef _LOOPUS_SIMPLIFYIDCALLS_H_INCLUDE_
#define _LOOPUS_SIMPLIFYIDCALLS_H_INCLUDE_

#include "ArgPromotionTracker.h"
#include "OCL/NameMangling.h"
#include "OpenCLMDKernels.h"

#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

class SimplifyIDCalls : public llvm::ModulePass {
  protected:
    ArgPromotionTracker *APT;
    OpenCLMDKernels *MDK;

    llvm::ConstantInt* getRequiredWorkGroupSizeConst(const llvm::Function *F,
        unsigned Dimension) const;

    bool replaceAggregateIDCalls(llvm::Function *F, llvm::Module *M);
    bool reduceIDCalls(llvm::Function *F);
    bool replaceConstIDCalls(llvm::Function *F);
    bool replaceConstIDArgs(llvm::Function *F);

  public:
    static char ID;

    SimplifyIDCalls(void);
    virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
    virtual bool runOnModule(llvm::Module &M) override;

    bool handleFunction(llvm::Function *F, llvm::Module *M);
};

#endif

