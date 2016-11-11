#include <sstream>
#include <memory>
#include <list>
#include <cxxabi.h>

#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include "HW/Arith.h"
#include "HW/Compare.h"
#include "HW/Constant.h"
#include "HW/Control.h"
#include "HW/Design.h"
#include "HW/HW.h"
#include "HW/Kernel.h"
#include "HW/Memory.h"
#include "HW/Streams.h"
#include "HW/typedefs.h"

#include "Vhdl.h"

#include "GenerateVhdl.h"
#include "OCLAccHWVisitor.h"
#include "OCLAccHW.h"
#include "OpenCLDefines.h"

#include "kernel_builtins.h"

#include "macros.h"

using namespace llvm;
using namespace oclacc;

INITIALIZE_PASS_BEGIN(GenerateVhdl, "oclacc-vhdl", "Generate VHDL from OCLAccHW",  false, true)
INITIALIZE_PASS_DEPENDENCY(OCLAccHW);
INITIALIZE_PASS_END(GenerateVhdl, "oclacc-vhdl", "Generate VHDL from OCLAccHW",  false, true)

char GenerateVhdl::ID = 0;

namespace llvm {
  Pass *createGenerateVhdlPass() { 
    return new GenerateVhdl(); 
  }
}

GenerateVhdl::GenerateVhdl() : ModulePass(GenerateVhdl::ID) {
  initializeGenerateVhdlPass(*PassRegistry::getPassRegistry());
  DEBUG_WITH_TYPE("GenerateVhdl", dbgs() << "GenerateVhdl created\n");
}

GenerateVhdl::~GenerateVhdl() {
}

bool GenerateVhdl::doInitialization(Module &M) {
  return false;
}

bool GenerateVhdl::doFinalization(Module &M) {
  return false;
}

void GenerateVhdl::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<OCLAccHW>();
  AU.setPreservesAll();
} 

GenerateVhdl *createGenerateVhdl() {
  return new GenerateVhdl();
}

bool GenerateVhdl::runOnModule(Module &M) {
  OCLAccHW &HWP = getAnalysis<OCLAccHW>();
  DesignUnit &Design = HWP.getDesign(); 
  DEBUG_WITH_TYPE("GenerateVhdl", dbgs() << "DesignUnit: " << Design.getName() << "\n");

  Vhdl V;
  Design.accept(V);

  return false;
}
