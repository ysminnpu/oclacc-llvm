//===- PrintModule.h - Print Module -----------------===//
//
//===----------------------------------------------------------------------===//

#ifndef PRINTMODULE_H
#define PRINTMODULE_H

#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

class PrintModule : public llvm::ModulePass {

  public:
    static char ID;

    PrintModule(void);
    virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
    virtual bool runOnModule(llvm::Module &M) override;
};

#endif /* PRINTMODULE_H */

