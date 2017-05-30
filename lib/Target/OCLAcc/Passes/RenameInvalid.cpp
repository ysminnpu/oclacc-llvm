#include <algorithm>

#include "llvm/Transforms/Scalar.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Pass.h"

using namespace llvm;

namespace {

struct RenameInvalid : public ModulePass {
  static char ID; // Pass identification, replacement for typeid
  RenameInvalid() : ModulePass(ID) {
    initializeRenameInvalidPass(*PassRegistry::getPassRegistry());
  }
  
  void getAnalysisUsage(AnalysisUsage &Info) const override {
    Info.setPreservesAll();
  }

  bool runOnModule(Module &M) override {
    for (GlobalVariable &G : M.getGlobalList()) {
      std::string Name = G.getName();
      std::replace(Name.begin(), Name.end(), '.', '_');
      G.setName(Name);
    }

    for (Function &F : M) {
      for (BasicBlock &BB : F) {
        for (Instruction &I : BB) {
          std::string Name = I.getName();
          std::replace(Name.begin(), Name.end(), '.', '_');
          I.setName(Name);
        }

        std::string Name = BB.getName();
        std::replace(Name.begin(), Name.end(), '.', '_');
        BB.setName(Name);
      }
    }
    return false;
  }
};
  
  char RenameInvalid::ID = 0;
}

INITIALIZE_PASS(RenameInvalid, "renameinv", 
                "Rename invalid names like 'or.cond' to 'or_cond'", false, false)
//===----------------------------------------------------------------------===//
//
// InstructionNamer - Give any unnamed non-void instructions "tmp" names.
//
namespace llvm {
Pass *createRenameInvalidPass() {
  return new RenameInvalid();
}
}
