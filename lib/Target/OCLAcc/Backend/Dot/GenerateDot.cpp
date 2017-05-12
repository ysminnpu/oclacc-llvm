#include <sstream>
#include <memory>
#include <list>
#include <cxxabi.h>

#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/CFG.h"
#include "llvm/Support/CommandLine.h"

#include "HW/Design.h"
#include "Dot.h"
#include "GenerateDot.h"
#include "OCLAccHWVisitor.h"
#include "OCLAccHW.h"

#define DEBUG_TYPE "dot"

using namespace llvm;
using namespace oclacc;

INITIALIZE_PASS_BEGIN(GenerateDot, "oclacc-dot", "Generate Dot from OCLAccHW",  false, true)
INITIALIZE_PASS_DEPENDENCY(OCLAccHW);
INITIALIZE_PASS_END(GenerateDot, "oclacc-dot", "Generate Dot from OCLAccHW",  false, true)

char GenerateDot::ID = 0;

namespace llvm {
  Pass *createGenerateDotPass() { 
    return new GenerateDot(); 
  }
}

GenerateDot::GenerateDot() : ModulePass(GenerateDot::ID) {
  initializeGenerateDotPass(*PassRegistry::getPassRegistry());
}

GenerateDot::~GenerateDot() {
}

bool GenerateDot::doInitialization(Module &M) {
  return false;
}

bool GenerateDot::doFinalization(Module &M) {
  return false;
}

void GenerateDot::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<OCLAccHW>();
  AU.setPreservesAll();
} 

GenerateDot *createGenerateDot() {
  return new GenerateDot();
}

bool GenerateDot::runOnModule(Module &M) {
  OCLAccHW &HWP = getAnalysis<OCLAccHW>();
  DesignUnit &Design = HWP.getDesign(); 

  Dot D;
  Design.accept(D);

  return false;
}

#undef DEBUG_TYPE
