//===-- Loopus.cpp --------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Loopus.h"
#include "llvm-c/Initialization.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/IR/Verifier.h"
#include "llvm/InitializePasses.h"
#include "llvm/PassManager.h"

using namespace llvm;

void llvm::initializeLoopus(PassRegistry &Registry) {
  initializeHDLLoopUnrollPass(Registry);
  initializeSimplifyIDCallsPass(Registry);
  initializeHDLPromoteIDPass(Registry);
  initializeHDLInlinerPass(Registry);
  initializeOpenCLMDKernelsPass(Registry);
  initializeHDLFlattenCFGPass(Registry);
  initializeSplitBarrierBlocksPass(Registry);
  initializeDelayStoresPass(Registry);
  initializeBitWidthAnalysisPass(Registry);
  initializeShiftRegisterDetectionPass(Registry);
  initializeRewriteExprPass(Registry);
  initializeAggregateLoadsPass(Registry);
  initializeInstSimplifyPass(Registry);
  initializeArgPromotionTrackerPass(Registry);
  initializeCanonicalizePredecessorsPass(Registry);
  initializeCFGOptimizerPass(Registry);
  initializeRenameInvalidPass(Registry);
}

