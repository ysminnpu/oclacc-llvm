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

#include "GenerateDotPass.h"
#include "OCLAccHWVisitor.h"
#include "OCLAccHWPass.h"
#include "OpenCLDefines.h"

#include "kernel_builtins.h"

#include "macros.h"

using namespace oclacc;

namespace llvm {

GenerateDotPass::GenerateDotPass() : ModulePass(GenerateDotPass::ID) {
  DEBUG_WITH_TYPE("GenerateDotPass", dbgs() << "GenerateDotPass created\n");
}

GenerateDotPass::~GenerateDotPass() {
}

bool GenerateDotPass::doInitialization(Module &M) {
  return false;
}

bool GenerateDotPass::doFinalization(Module &M) {
  return false;
}

void GenerateDotPass::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<OCLAccHWPass>();
  AU.setPreservesAll();
} 

GenerateDotPass *createGenerateDotPass() {
  return new GenerateDotPass();
}

bool GenerateDotPass::runOnModule(Module &M) {
  OCLAccHWPass &HWP = getAnalysis<OCLAccHWPass>();
  DesignUnit &Design = HWP.getDesign(); 

  DEBUG_WITH_TYPE("GenerateDotPass", dbgs() << "DesignUnit: " << Design.getName() << "\n");

  return false;
}

char GenerateDotPass::ID = 0;

static RegisterPass<GenerateDotPass> X("oclacc-generate-dot", "Generate Dot");

} //end namespace llvm
