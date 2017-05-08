//===- HDLInliner.h - Inlines non-kernel functions into kernel functions --===//
//
// Replaces callsites in kernel functions to non-kernel functions by the body of
// the function itself.
//
//===----------------------------------------------------------------------===//

#ifndef _LOOPUS_HDLINLINER_H_INCLUDE_
#define _LOOPUS_HDLINLINER_H_INCLUDE_

#include "OpenCLMDKernels.h"

#include "llvm/Analysis/InlineCost.h"
#include "llvm/Transforms/IPO/InlinerPass.h"

class HDLInliner : public llvm::Inliner {
  private:
    llvm::InlineCostAnalysis *ICA;
    OpenCLMDKernels *OCK;

  public:
    static char ID;

    HDLInliner();
    void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
    bool runOnSCC(llvm::CallGraphSCC &SCC) override;

    llvm::InlineCost getInlineCost(llvm::CallSite CS) override;
};

#endif

