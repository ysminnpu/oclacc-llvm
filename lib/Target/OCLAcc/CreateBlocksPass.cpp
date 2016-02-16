#include <sstream>
#include <memory>
#include <list>
#include <cxxabi.h>

#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/CFG.h"

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

#include "CreateBlocksPass.h"
#include "OCLAccHWVisitor.h"
#include "OCLAccHWPass.h"
#include "OpenCLDefines.h"

#include "kernel_builtins.h"

#include "todo.h"

namespace llvm {

// Helper Functions
bool CreateBlocksPass::isDefInCurrentBB(Value *V) {
  bool ret = false;
  if (Instruction *VI = dyn_cast<Instruction>(V)) {
    BasicBlock *ValBB = VI->getParent();
    if (ValBB == CurrentBB) 
      ret = true;
  }
  return ret;
}


CreateBlocksPass::CreateBlocksPass() : FunctionPass(CreateBlocksPass::ID) {
  DEBUG_WITH_TYPE("CreateBlocksPass", dbgs() << "CreateBlocksPass created\n");
}

CreateBlocksPass::~CreateBlocksPass() {
}

bool CreateBlocksPass::doInitialization(Module &F) {
  return false;
}

bool CreateBlocksPass::doFinalization(Module &F) {
  return false;
}

void CreateBlocksPass::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
} 

bool CreateBlocksPass::runOnFunction(Function &F) {
  visit(F);
  return false;
}

CreateBlocksPass *createCreateBlocksPass() {
  return new CreateBlocksPass();
}

void CreateBlocksPass::visitBasicBlock(BasicBlock &B) {
  DEBUG_WITH_TYPE("CreateBlocksPass", dbgs() << "Found block " << B.getName() << "\n");

  oclacc::block_p HWB = std::make_shared<oclacc::Block>(B.getName());
  Blocks.push_back(HWB);
}

char CreateBlocksPass::ID = 0;

static RegisterPass<CreateBlocksPass> X("oclacc-create-blocks", 
    "Creates Blocks from BBs.");

} //end namespace llvm
