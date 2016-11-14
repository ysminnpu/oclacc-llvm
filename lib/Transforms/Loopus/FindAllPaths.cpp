//===- FindAllPaths.cpp - Implementation of FindAllPaths Pass -------------===//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "hdl-findallpaths"

#include "FindAllPaths.h"

#include "llvm/IR/CFG.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include <algorithm>

using namespace llvm;


const FindAllPaths::PathTy FindAllPaths::getPathFromTo(const BasicBlock *F, const BasicBlock *T) {
  FindAllPaths::PathTy P;

  for (FindAllPaths::SinglePathTy &SP : Paths) {
    FindAllPaths::SinglePathConstIt FIt = std::find(SP.begin(), SP.end(), F); 
    FindAllPaths::SinglePathConstIt TIt = std::find(SP.begin(), SP.end(), T); 

    // Check if path contains F and T
    if (FIt == SP.end() || TIt == SP.end()) continue;


    // Add the subpath to P
    const FindAllPaths::SinglePathTy NP(FIt, std::next(TIt));
    P.push_back(NP);
  }

  return P;
}

void FindAllPaths::walkPaths(const BasicBlock *BB) {
  succ_const_iterator I = succ_begin(BB), E = succ_end(BB); 

  // Leaf node
  if (I == E) {
    FindAllPaths::SinglePathTy NP;
    NP.insert(NP.end(), Paths.back().begin(), Paths.back().end());

    Paths.push_back(NP);
  } else {
    // While we can descend, we append the current block BB to the currently 
    // longest path.
    for (; I != E; ++I) {
      Paths.back().push_back(*I);
      walkPaths(*I);
      Paths.back().pop_back();
    }
  }
}

//===- Implementation of LLVM pass ----------------------------------------===//
INITIALIZE_PASS(FindAllPaths, "loopus-paths", "Find all paths between two BasicBlocks",  false, false)

char FindAllPaths::ID = 0;

namespace llvm {
  Pass* createFindAllPathsPass() {
    return new FindAllPaths();
  }
}

FindAllPaths::FindAllPaths() : FunctionPass(ID){
  initializeFindAllPathsPass(*PassRegistry::getPassRegistry());
}

void FindAllPaths::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
}

bool FindAllPaths::runOnFunction(Function &F) {
  Paths.clear();

  FindAllPaths::WorkListTy WL;
  const BasicBlock *EntryBB = &(F.getEntryBlock());
  WL.push_back(EntryBB);

  FindAllPaths::SinglePathTy P;
  P.push_back(EntryBB);

  Paths.push_back(P);
  FindAllPaths::walkPaths(EntryBB);
  
  return false;
}

bool FindAllPaths::doInitialization(Module &F) {
  return false;
}

