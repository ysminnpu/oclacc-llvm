//===- FindAllPaths.cpp - Implementation of FindAllPaths Pass -------------===//
//===----------------------------------------------------------------------===//

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

#define DEBUG_TYPE "loopus-paths"

using namespace llvm;

/// \brief
///
/// \param F
/// \param T
/// \param V
const FindAllPaths::PathTy FindAllPaths::getPathForValue(
    const BasicBlock *F, const BasicBlock *T, const Value *V) const {
  
  assert(V->isUsedInBasicBlock(T));
  assert(F != T);

  // Check by which Instructions Value V is used. If it is only inside of
  // PHINodes, we can eliminate all other Paths. If a Value is used directy,
  // i.e. outside of PHINodes, it must have been defined in a dominating
  // BasicBlock and has to be propagated through all possible Paths along
  // definition and use.
  
  bool OnlyUsedByPHI = true;
  
  // Implementation similar to Value::isUsedInBasicBlock(const BasicBlock *BB)
  // We do not stop with the first Use.
  //
	BasicBlock::const_iterator BI = T->begin(), BE = T->end();
  Value::const_user_iterator UI = V->user_begin(), UE = V->user_end();
	for (; BI != BE && UI != UE; ++BI, ++UI) {
		// Scan basic block: Check if this Value is used by the instruction at BI.
		if (std::find(BI->op_begin(), BI->op_end(), V) != BI->op_end()) {
      if (!isa<PHINode>(*BI)) {
        OnlyUsedByPHI = false;
        break;
      }
    }
		// Scan use list: Check if the use at UI is in BB.
		const Instruction *User = dyn_cast<Instruction>(*UI);
		if (!isa<PHINode>(User) && User->getParent() == T) {
      OnlyUsedByPHI = false;
      break;
    }
	}

  DEBUG(
    if (OnlyUsedByPHI) {
      dbgs() << "Value " << V->getName() << " in BB " << T->getName() << " is pure PHI input\n";
    }
  );

  // If the Value is used by normal Instructions, it must be passed through all
  // Blocks in between F and T
  //
  if (!OnlyUsedByPHI) {
    return getPathFromTo(F, T);
  }

  FindAllPaths::PathTy P;

  // TODO Investigate if Paths are added multiple times, e.g. for B1->B3 with
  // the following Paths available:
  // B0 -> B1 -> B2 -> B3 -> B4: B1->B2->B3
  // B0 -> B1 -> B2 -> B3 -> B5: B1->B2->B3
  for (const FindAllPaths::SinglePathTy &SP : Paths) {
    FindAllPaths::SinglePathConstIt FIt = std::find(SP.begin(), SP.end(), F); 
    FindAllPaths::SinglePathConstIt TIt = std::find(SP.begin(), SP.end(), T); 

    // Check if the Path contains F and T and is possibly valid
    if (FIt == SP.end() || TIt == SP.end()) continue;

    // The Value V is only used by PHINodes, so add the Paths with the
    // incoming Blocks of the PHINode using the Value and skip all others.
    for (const Instruction &I : T->getInstList() ) {
      const PHINode *PHI = dyn_cast<PHINode>(&I);
      if (!PHI) {
        // Current Path investigated, move on to next Path
        break;
      }

      // Add the subpath to P if the previous Block in SP is in the incoming Block
      // for Value V

      for (unsigned i = 0; i < PHI->getNumIncomingValues(); ++i) {
        const Value *IncV = PHI->getIncomingValue(i);
        const BasicBlock *IncBB = PHI->getIncomingBlock(i);

        if (IncV == V) {
          // We start with F because the Path may simply be F -> T
          const BasicBlock *PrevBB = F;

          FindAllPaths::SinglePathConstReverseIt PrevIt = std::find(SP.rbegin(), SP.rend(), T);
          PrevIt++;

          if (PrevIt != SP.rend())
            PrevBB = *PrevIt;

  //        outs() << "previous BB in Path: " << PrevBB->getName() << "\n";
              
          if (PrevBB == IncBB) {
   //         outs() << "Previous BB is in PHI Incoming List:" << PrevBB->getName() << "\n";

            const FindAllPaths::SinglePathTy NP(FIt, std::next(TIt));
            P.push_back(NP);
          } else {
     //       outs() << "Skip previous BB " << PrevBB->getName() << " for incoming BB " << IncBB->getName() << "\n";
          }
        }
      }
    }
  }
  // After investigating all PHINodes at the beginning of T, we must have
  // added Paths to P
  assert(!P.empty());

  return P;
}

const FindAllPaths::PathTy FindAllPaths::getPathFromTo(const BasicBlock *F, const BasicBlock *T) const {
  FindAllPaths::PathTy P;

  for (const FindAllPaths::SinglePathTy &SP : Paths) {
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

/// \brief Recursively create Path List
///
/// Paths.back() always contains the current Path. If we reach a leaf, duplicate
/// it and append it to the back. The last Path will
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

void FindPaths::dump(FindAllPaths::SinglePathTy P) {
  for (FindAllPaths::SinglePathConstIt FromBBIt = P.begin(), ToBBIt = std::next(FromBBIt); 
      ToBBIt != P.end(); 
      FromBBIt = ToBBIt, ++ToBBIt ) {
    outs() << "  " << (*FromBBIt)->getName() << " -> " << (*ToBBIt)->getName() << "\n";
  }
}

void FindPaths::dump(FindAllPaths::PathTy PP) {
  outs() << "-----------------\n";
  int i = 0;
  for (const FindAllPaths::SinglePathTy &P : PP) {
    outs() << "Path " << i << ":\n";
    FindPaths::dump(P);
    ++i;
  }
  outs() << "-----------------\n";
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

  const BasicBlock *EntryBB = &(F.getEntryBlock());

  // walkPaths exxpects Paths to contain a SinglePlath with a Block to start
  // from
  FindAllPaths::SinglePathTy P;
  P.push_back(EntryBB);

  Paths.push_back(P);

  walkPaths(EntryBB);

  // The final Path sill ontains the initial block. We can remove it.
  if (Paths.back().back() == EntryBB)
    Paths.pop_back();

  DEBUG(dbgs() << "Generated Paths:\n");
  DEBUG(FindPaths::dump(Paths));

  return false;
}

bool FindAllPaths::doInitialization(Module &F) {
  return false;
}

#undef DEBUG_TYPE

