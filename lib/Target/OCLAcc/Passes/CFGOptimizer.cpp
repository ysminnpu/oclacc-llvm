//===- CFGOptimizer.cpp ---------------------------------------------------===//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "loopus-optcfg"

#include "CFGOptimizer.h"

#include "CanonicalizePredecessors.h"
#include "DelayStores.h"
#include "HDLFlattenCFG.h"

#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

STATISTIC(StatsNumIterationsCFGOpt, "Number of CFG optimization iterations.");

//===- Implementation of pass functions -----------------------------------===//
INITIALIZE_PASS_BEGIN(CFGOptimizer, "loopus-optcfg", "Optimize CFG", true, false)
INITIALIZE_AG_DEPENDENCY(AliasAnalysis)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_DEPENDENCY(DataLayoutPass)
INITIALIZE_PASS_END(CFGOptimizer, "loopus-optcfg", "Optimize CFG", true, false)

char CFGOptimizer::ID = 0;

namespace llvm {
  Pass* createCFGOptimizerPass() {
    return new CFGOptimizer();
  }
}

CFGOptimizer::CFGOptimizer(void)
 : FunctionPass(ID) {
  initializeCFGOptimizerPass(*PassRegistry::getPassRegistry());
}

void CFGOptimizer::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<AliasAnalysis>();
  AU.addRequired<DominatorTreeWrapperPass>();
  AU.addRequired<DataLayoutPass>();
  AU.addPreserved<DataLayoutPass>();
}

bool CFGOptimizer::runOnFunction(Function &F) {
  AliasAnalysis *AA = &getAnalysis<AliasAnalysis>();
  DominatorTree *DT = &getAnalysis<DominatorTreeWrapperPass>().getDomTree();
  DataLayoutPass *DLP = &getAnalysis<DataLayoutPass>();
  const DataLayout *DL = nullptr;
  if (DLP != nullptr) {
    DL = &DLP->getDataLayout();
  }
  LoopInfo *LI = getAnalysisIfAvailable<LoopInfo>();
  ScalarEvolution *SE = getAnalysisIfAvailable<ScalarEvolution>();

  bool GlobalChanged = false, LocalChanged = false;
  do {
    LocalChanged = false;
    {   // Insert missing tail blocks
      DEBUG(dbgs() << "Running tail block injection...\n");
      LocalChanged |= Loopus::CanonicalizePredecessorsWorker(DT, LI).runOnFunction(&F);
    } { // Hoist loads and sink stores
      DEBUG(dbgs() << "Running delay store...\n");
      LocalChanged |= Loopus::DelayStoresWorker(AA).runOnFunction(&F);
    } { // Flatten CFG and merge blocks
      DEBUG(dbgs() << "Running cfg flattener...\n");
      Loopus::SimpleRessourceEstimator SRE;
      LocalChanged |= Loopus::HDLFlattenCFGWorker(AA, DL, DT, LI, this, SE, &SRE).runOnFunction(&F);
    }
    GlobalChanged |= LocalChanged;
    ++StatsNumIterationsCFGOpt;
  } while (LocalChanged == true);

  return GlobalChanged;
}
