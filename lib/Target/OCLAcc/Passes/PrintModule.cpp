//===- PrintModule.cpp - Implementation of PrintModule pass -------===//
//===----------------------------------------------------------------------===//

#include "PrintModule.h"


#include "llvm/IR/PassManager.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/IR/IRPrintingPasses.h"

#include <set>

using namespace llvm;

//===- Implementation of LLVM pass ----------------------------------------===//
INITIALIZE_PASS_BEGIN(PrintModule, "oclacc-printmodule", "Print current module to file",  false, true)
INITIALIZE_PASS_END(PrintModule, "oclacc-printmodule", "Print current module to file",  false, true)

char PrintModule::ID = 0;

namespace llvm {
  Pass* createPrintModulePass() {
    return new PrintModule();
  }
}

PrintModule::PrintModule(void) : ModulePass(ID) {
  initializePrintModulePass(*PassRegistry::getPassRegistry());
}

void PrintModule::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
}

/// \brief Searches the metadata nodes for available kernel functions.
///
/// Inspects the "opencl.kernels" metadata node and its subnodes and searches
/// for available kernel functions in the currently processed module. The found
/// kernel functions can be retrieved by using the \c getDefinedKernels and the
/// \c getAllKernels functions.
bool PrintModule::runOnModule(Module &M) {
  // Print current state of optimization
  std::error_code EC;
  std::string FileName = M.getName().str()+".final.ll";
  raw_fd_ostream File(FileName, EC, llvm::sys::fs::F_RW | llvm::sys::fs::F_Text);
  if (EC) {
    errs() << "Failed to create " << FileName << "(" << __LINE__ << "): " << EC.message() << "\n";
    return -1;
  }
  PrintModulePass PrintPass(File);
  PrintPass.run(M);

  return false;
}
