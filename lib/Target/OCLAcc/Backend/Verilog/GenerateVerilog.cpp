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

#include "Backend/Verilog/Verilog.h"

#include "GenerateVerilog.h"
#include "OCLAccHWVisitor.h"
#include "OCLAccHW.h"
#include "OpenCLDefines.h"

#include "kernel_builtins.h"

#include "macros.h"

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
  DEBUG_WITH_TYPE("GenerateVerilog", dbgs() << "GenerateVerilog created\n");
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

  Verilog V;
  DEBUG_WITH_TYPE("GenerateVerilog", dbgs() << "DesignUnit: " << Design.getName() << "\n");

  //VeriloVisitor V;
  //Design.accept(V);

  return false;
}
