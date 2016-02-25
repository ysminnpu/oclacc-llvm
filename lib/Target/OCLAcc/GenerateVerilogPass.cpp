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
#include "HW/operators.cpp"

#include "GenerateVerilogPass.h"
#include "OCLAccHWVisitor.h"
#include "OCLAccHWPass.h"
#include "OpenCLDefines.h"

#include "kernel_builtins.h"

#include "todo.h"

namespace llvm {

GenerateVerilogPass::GenerateVerilogPass() : ModulePass(GenerateVerilogPass::ID) {
  DEBUG_WITH_TYPE("GenerateVerilogPass", dbgs() << "GenerateVerilogPass created\n");
}

GenerateVerilogPass::~GenerateVerilogPass() {
}

bool GenerateVerilogPass::doInitialization(Module &M) {
  return false;
}

bool GenerateVerilogPass::doFinalization(Module &M) {
  return false;
}

void GenerateVerilogPass::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<OCLAccHWPass>();
  AU.setPreservesAll();
} 

GenerateVerilogPass *createGenerateVerilogPass() {
  return new GenerateVerilogPass();
}

bool GenerateVerilogPass::runOnModule(Module &M) {
  OCLAccHWPass &HWP = getAnalysis<OCLAccHWPass>();
  DesignUnit &Design = HWP.getDesign(); 
  DEBUG_WITH_TYPE("GenerateVerilogPass", dbgs() << "DesignUnit: " << Design.getName() << "\n");

  //VeriloVisitor V;
  //Design.accept(V);

  return false;
}

char GenerateVerilogPass::ID = 0;

static RegisterPass<GenerateVerilogPass> X("oclacc-generate-verilog", 
    "Generate Verilog");

} //end namespace llvm
