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

#include "HW/HW.h"
#include "HW/typedefs.h"
#include "HW/Arith.h"
#include "HW/Constant.h"
#include "HW/Compare.h"
#include "HW/Control.h"
#include "HW/Kernel.h"
#include "HW/Memory.h"
#include "HW/Streams.h"

#include "Backend/Dot/Dot.h"

#include "GenerateDot.h"
#include "OCLAccHWVisitor.h"
#include "OCLAccHW.h"
#include "OpenCLDefines.h"

#include "kernel_builtins.h"

#include "macros.h"

using namespace llvm;
using namespace oclacc;

INITIALIZE_PASS_BEGIN(GenerateDot, "GenerateDot", "foo",  false, false)
INITIALIZE_PASS_DEPENDENCY(OCLAccHW);
INITIALIZE_PASS_END(GenerateDot, "GenerateDot", "foo",  false, false)

char GenerateDot::ID = 0;

namespace llvm {
  Pass *createGenerateDotPass() { 
    return new GenerateDot(); 
  }
}

GenerateDot::GenerateDot() : ModulePass(GenerateDot::ID) {
  initializeGenerateDotPass(*PassRegistry::getPassRegistry());
  DEBUG_WITH_TYPE("GenerateDot", dbgs() << "GenerateDot created\n");
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

  DEBUG_WITH_TYPE("GenerateDot", dbgs() << "DesignUnit: " << Design.getName() << "\n");

  return false;
}
