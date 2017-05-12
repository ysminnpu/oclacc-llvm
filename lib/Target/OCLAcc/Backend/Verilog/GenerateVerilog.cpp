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
#include "Verilog.h"
#include "GenerateVerilog.h"
#include "OCLAccHWVisitor.h"
#include "OCLAccHW.h"
#include "OCL/OpenCLDefines.h"
#include "FlopocoFPFormat.h"

#define DEBUG_TYPE "verilog"

using namespace llvm;
using namespace oclacc;

INITIALIZE_PASS_BEGIN(GenerateVerilog, "oclacc-verilog", "Generate Verilog from OCLAccHW",  false, true)
INITIALIZE_PASS_DEPENDENCY(OCLAccHW);
INITIALIZE_PASS_END(GenerateVerilog, "oclacc-verilog", "Generate Verilog from OCLAccHW",  false, true)

char GenerateVerilog::ID = 0;

namespace llvm {
  Pass *createGenerateVerilogPass() { 
    return new GenerateVerilog(); 
  }
}

GenerateVerilog::GenerateVerilog() : ModulePass(GenerateVerilog::ID) {
  initializeGenerateVerilogPass(*PassRegistry::getPassRegistry());
}

GenerateVerilog::~GenerateVerilog() {
}

bool GenerateVerilog::doInitialization(Module &M) {
  return false;
}

bool GenerateVerilog::doFinalization(Module &M) {
  return false;
}

void GenerateVerilog::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<OCLAccHW>();
  AU.setPreservesAll();
} 

GenerateVerilog *createGenerateVerilog() {
  return new GenerateVerilog();
}

bool GenerateVerilog::runOnModule(Module &M) {
  OCLAccHW&HWP = getAnalysis<OCLAccHW>();
  DesignUnit &Design = HWP.getDesign(); 

  FlopocoFPFormat F;
  Design.accept(F);

  Verilog V;
  Design.accept(V);

  return false;
}

#undef DEBUG_TYPE
