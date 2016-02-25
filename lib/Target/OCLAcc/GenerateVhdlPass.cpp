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
#include "HW/Visitor/Vhdl.h"
#include "HW/operators.cpp"
#include "HW/typedefs.h"

#include "GenerateVhdlPass.h"
#include "OCLAccHWVisitor.h"
#include "OCLAccHWPass.h"
#include "OpenCLDefines.h"

#include "kernel_builtins.h"

#include "todo.h"


namespace llvm {

GenerateVhdlPass::GenerateVhdlPass() : ModulePass(GenerateVhdlPass::ID) {
  DEBUG_WITH_TYPE("GenerateVhdlPass", dbgs() << "GenerateVhdlPass created\n");
}

GenerateVhdlPass::~GenerateVhdlPass() {
}

bool GenerateVhdlPass::doInitialization(Module &M) {
  return false;
}

bool GenerateVhdlPass::doFinalization(Module &M) {
  return false;
}

void GenerateVhdlPass::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<OCLAccHWPass>();
  AU.setPreservesAll();
} 

GenerateVhdlPass *createGenerateVhdlPass() {
  return new GenerateVhdlPass();
}

bool GenerateVhdlPass::runOnModule(Module &M) {
  OCLAccHWPass &HWP = getAnalysis<OCLAccHWPass>();
  DesignUnit &Design = HWP.getDesign(); 
  DEBUG_WITH_TYPE("GenerateVhdlPass", dbgs() << "DesignUnit: " << Design.getName() << "\n");

  vhdl::VhdlVisitor V;
  Design.accept(V);

  return false;
}

char GenerateVhdlPass::ID = 0;

static RegisterPass<GenerateVhdlPass> X("oclacc-generate-vhdl", 
    "Generate VHDL");

} //end namespace llvm
