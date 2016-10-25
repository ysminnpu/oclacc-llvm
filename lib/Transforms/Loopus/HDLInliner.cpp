//===- HDLInliner.cpp - Implementation of the HDLInliner pass -------------===//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "hdl-inliner"

#include "HDLInliner.h"

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

//===- Implementation of LLVM pass ----------------------------------------===//
INITIALIZE_PASS_BEGIN(HDLInliner, "loopus-inline", "Inline function calls", true, false)
INITIALIZE_AG_DEPENDENCY(AliasAnalysis)
INITIALIZE_PASS_DEPENDENCY(OpenCLMDKernels)
INITIALIZE_PASS_DEPENDENCY(AssumptionCacheTracker)
INITIALIZE_PASS_DEPENDENCY(CallGraphWrapperPass)
INITIALIZE_PASS_DEPENDENCY(InlineCostAnalysis)
INITIALIZE_PASS_END(HDLInliner, "loopus-inline", "Inline function calls", true, false)

char HDLInliner::ID = 0;

namespace llvm {
  Pass* createHDLInlinerPass() {
    return new HDLInliner();
  }
}

HDLInliner::HDLInliner()
 : Inliner(ID, -2000000000, /*Not quite sure about this*/false),
   ICA(0), OCK(0) {
  initializeHDLInlinerPass(*PassRegistry::getPassRegistry());
}

void HDLInliner::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<InlineCostAnalysis>();
  AU.addRequired<OpenCLMDKernels>();
  AU.addPreserved<OpenCLMDKernels>();
  Inliner::getAnalysisUsage(AU);
}

InlineCost HDLInliner::getInlineCost(CallSite CS) {
  Function *F = CS.getCalledFunction();
  if (F == 0) { return InlineCost::getNever(); }

  if (F->isDeclaration() == true) {
    DEBUG(dbgs() << "Function: " << F->getName() << ": cannot inline declaration!\n");
    return InlineCost::getNever();
  }
  if (ICA->isInlineViable(*F) == false) {
    DEBUG(dbgs() << "Function: " << F->getName() << ": cannot be inlined!\n");
    return InlineCost::getNever();
  }
  if (OCK->isKernel(F) == true) {
    DEBUG(dbgs() << "Function: " << F->getName() << ": will not inline kernel!\n");
    return InlineCost::getNever();
  }
  if (CS.hasFnAttr(Attribute::NoInline) == true) {
    DEBUG(dbgs() << "Function: " << F->getName() << ": marked not to be inlined!\n");
    return InlineCost::getNever();
  }

  // Function can and should be inlined so return getAlways()
  return InlineCost::getAlways();
}

bool HDLInliner::runOnSCC(CallGraphSCC &SCC) {
  ICA = &getAnalysis<InlineCostAnalysis>();
  OCK = &getAnalysis<OpenCLMDKernels>();

  if ((ICA == nullptr) || (OCK == nullptr)) {
    return false;
  }

  return Inliner::runOnSCC(SCC);
}
