#include <algorithm>

#include "llvm/Transforms/Scalar.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"
#include "llvm/Pass.h"

using namespace llvm;

namespace {
  struct RenameInvalid : public FunctionPass {
    static char ID; // Pass identification, replacement for typeid
    RenameInvalid() : FunctionPass(ID) {
      initializeRenameInvalidPass(*PassRegistry::getPassRegistry());
    }
    
    void getAnalysisUsage(AnalysisUsage &Info) const override {
      Info.setPreservesAll();
    }

    bool runOnFunction(Function &F) override {
      for (BasicBlock &BB : F) {
        
        for (Instruction &I : BB) {
          std::string name = I.getName();
          std::replace(name.begin(), name.end(), '.', '_');
          I.setName(name);
        }
      }
      return true;
    }
  };
  
  char RenameInvalid::ID = 0;
}

INITIALIZE_PASS(RenameInvalid, "renameinv", 
                "Rename invalid names like 'or.cond'", false, false)
//===----------------------------------------------------------------------===//
//
// InstructionNamer - Give any unnamed non-void instructions "tmp" names.
//
namespace llvm {
Pass *createRenameInvalidPass() {
  return new RenameInvalid();
}
}
