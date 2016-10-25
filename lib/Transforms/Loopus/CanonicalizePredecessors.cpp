//===- CanonicalizePredecessors.cpp ---------------------------------------===//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "loopus-canonpreds"

#include "CanonicalizePredecessors.h"

#include "LoopusUtils.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

STATISTIC(StatsNumTailBlocksInjected, "Number of created and injected tail blocks.");

//===- Implementation of worker class -------------------------------------===//
Loopus::CanonicalizePredecessorsWorker::CanonicalizePredecessorsWorker(
    DominatorTree *DomTree, LoopInfo *LoIn)
 : DT(DomTree), LI(LoIn) {
}

bool Loopus::CanonicalizePredecessorsWorker::runOnBasicBlock(BasicBlock *BB) {
  if (BB == nullptr) { return false; }
  struct InjectPattern curPattern;
  if (isInjectPatternHead(BB, &curPattern) == true) {
    return injectDedicatedTail(&curPattern);
  }
  return false;
}

bool Loopus::CanonicalizePredecessorsWorker::runOnFunction(Function *F) {
  if (F == nullptr) { return false; }
  bool Changed = false;
  for (Function::iterator BBIT = F->begin(), BBEND = F->end(); BBIT != BBEND; ++BBIT) {
    BasicBlock *BB = &*BBIT;
    Changed |= runOnBasicBlock(BB);
  }
  return Changed;
}

bool Loopus::CanonicalizePredecessorsWorker::isInjectPatternHead(BasicBlock *BB, struct InjectPattern *Pattern) {
  if (BB == nullptr) {
    return false;
  }

  TerminatorInst *BBTI = BB->getTerminator();
  if (BBTI == nullptr) { return false; }
  if (BBTI->getNumSuccessors() != 2) { return false; }
  BranchInst *BBBI = dyn_cast<BranchInst>(BBTI);
  if (BBBI == nullptr) { return false; }

  BasicBlock *Succ0 = BBBI->getSuccessor(0);
  BasicBlock *Succ1 = BBBI->getSuccessor(1);
  if ((Succ0 == nullptr) || (Succ1 == nullptr)) { return false; }

  TerminatorInst *Succ0TI = Succ0->getTerminator();
  TerminatorInst *Succ1TI = Succ1->getTerminator();
  if ((Succ0TI == nullptr) || (Succ1TI == nullptr)) { return false; }

  const unsigned Succ0NumSuccs = Succ0TI->getNumSuccessors();
  const unsigned Succ1NumSuccs = Succ1TI->getNumSuccessors();

  // The head branches always into the same successor
  if (Succ0 == Succ1) {
    return false;
    #if 0
    if ((Succ0NumSucc == 1) && (Succ1NumSuccs == 1)) {
      if (Pattern != nullptr) {
        Pattern->HeadBB = BB;
        Pattern->LeftPred.BB = BB;
        Pattern->LeftPred.NumSucc = 0;
        Pattern->RightPred.BB = BB;
        Pattern->RightPred.NumSucc = 1;
        Pattern->TailBB = Succ0;
      }
      return true;
    }
    #endif
  }

  if ((Succ0NumSuccs != 1) && (Succ1NumSuccs != 1)) {
    // Nothing we can do here...
    return false;
  } else if ((Succ0NumSuccs == 1) && (Succ1NumSuccs != 1)) {
    // Head and Succ0 must branch into Succ1
    if (Succ0TI->getSuccessor(0) == Succ1) {
      if (Succ0->getSinglePredecessor() == BB) {
        if (Pattern != nullptr) {
          Pattern->HeadBB = BB;
          Pattern->LeftPred.BB = Succ0;
          Pattern->LeftPred.NumSucc = 0;
          Pattern->RightPred.BB = BB;
          Pattern->RightPred.NumSucc = 1;
          Pattern->TailBB = Succ1;
        }
        return true;
      }
    }
  } else if ((Succ0NumSuccs != 1) && (Succ1NumSuccs == 1)) {
    // Head and Succ1 must branch into Succ0
    if (Succ1TI->getSuccessor(0) == Succ0) {
      if (Succ1->getSinglePredecessor() == BB) {
        if (Pattern != nullptr) {
          Pattern->HeadBB = BB;
          Pattern->LeftPred.BB = BB;
          Pattern->LeftPred.NumSucc = 0;
          Pattern->RightPred.BB = Succ1;
          Pattern->RightPred.NumSucc = 0;
          Pattern->TailBB = Succ0;
        }
        return true;
      }
    }
  } else if ((Succ0NumSuccs == 1) && (Succ1NumSuccs == 1)) {
    if (Succ0TI->getSuccessor(0) == Succ1) {
      // Now - again - Head and Succ0 are branching into Succ1
      if (Succ0->getSinglePredecessor() == BB) {
        if (Pattern != nullptr) {
          Pattern->HeadBB = BB;
          Pattern->LeftPred.BB = Succ0;
          Pattern->LeftPred.NumSucc = 0;
          Pattern->RightPred.BB = BB;
          Pattern->RightPred.NumSucc = 1;
          Pattern->TailBB = Succ1;
        }
        return true;
      }
    } else if (Succ1TI->getSuccessor(0) == Succ0) {
      // Now - again - Head and Succ1 are branching into Succ0
      if (Succ1->getSinglePredecessor() == BB) {
        if (Pattern != nullptr) {
          Pattern->HeadBB = BB;
          Pattern->LeftPred.BB = BB;
          Pattern->LeftPred.NumSucc = 0;
          Pattern->RightPred.BB = Succ1;
          Pattern->RightPred.NumSucc = 0;
          Pattern->TailBB = Succ0;
        }
        return true;
      }
    } else if (Succ0TI->getSuccessor(0) == Succ1TI->getSuccessor(0)) {
      // Now Succ0 and Succ1 are branching into the same block
      if ((Succ0->getSinglePredecessor() == BB) && (Succ1->getSinglePredecessor() == BB)) {
        if (Pattern != nullptr) {
          Pattern->HeadBB = BB;
          Pattern->LeftPred.BB = Succ0;
          Pattern->LeftPred.NumSucc = 0;
          Pattern->RightPred.BB = Succ1;
          Pattern->RightPred.NumSucc = 0;
          Pattern->TailBB = Succ0TI->getSuccessor(0);
        }
        return true;
      }
    }
  }
  return false;
}

void Loopus::CanonicalizePredecessorsWorker::updateDomTree(struct InjectPattern *Pattern,
    BasicBlock *NewTail) {
  if ((Pattern == nullptr) || (NewTail == nullptr)) { return; }
  if (DT == nullptr) { return; }

  // Normally the left and right block should not have any blocks that they do
  // dominate. They are both branching into the tail block and therefore there
  // are at least two ways to reach the tail. So neither left nor right block
  // can be dominator of tail.
  DomTreeNode *RTDTN = DT->getRootNode();
  DomTreeNode *HeadDTN = DT->getNode(Pattern->HeadBB);
  if (HeadDTN == nullptr) { return; }
  DomTreeNode *RightBBDTN = DT->getNode(Pattern->RightPred.BB);
  if (RightBBDTN == nullptr) { return; }

  // Insert dominator tree node for the new tail block
  BasicBlock *NewTailDomBB = DT->findNearestCommonDominator(Pattern->LeftPred.BB, Pattern->RightPred.BB);
  if (NewTailDomBB == nullptr) { return; }
  DomTreeNode *NewTailDTN = DT->getNode(NewTail);
  if (NewTailDTN != nullptr) {
    DT->changeImmediateDominator(NewTailDTN, DT->getNode(NewTailDomBB));
  } else {
    NewTailDTN = DT->addNewBlock(NewTail, NewTailDomBB);
  }

  if ((Pattern->LeftPred.BB != Pattern->HeadBB) && (Pattern->LeftPred.BB != Pattern->TailBB)) {
    DomTreeNode *LeftBBDTN = DT->getNode(Pattern->LeftPred.BB);
    if ((LeftBBDTN != nullptr) && (LeftBBDTN != RTDTN) && (LeftBBDTN != HeadDTN)) {
      SmallVector<DomTreeNode*, 4> LeftDTNChildren(LeftBBDTN->begin(), LeftBBDTN->end());
      if (LeftDTNChildren.size() > 0) {
        DEBUG(dbgs() << ">>Why does the left block " << Pattern->LeftPred.BB->getName()
                     << " dominate any other blocks...?!?!\n");
        for (DomTreeNode *DTN : LeftDTNChildren) {
          DT->changeImmediateDominator(DTN, NewTailDTN);
        }
      }
    }
  }

  if ((Pattern->RightPred.BB != Pattern->HeadBB) && (Pattern->RightPred.BB != Pattern->TailBB)) {
    DomTreeNode *RightBBDTN = DT->getNode(Pattern->RightPred.BB);
    if ((RightBBDTN != nullptr) && (RightBBDTN != RTDTN) && (RightBBDTN != HeadDTN)) {
      SmallVector<DomTreeNode*, 4> RightDTNChildren(RightBBDTN->begin(), RightBBDTN->end());
      if (RightDTNChildren.size() > 0) {
        DEBUG(dbgs() << ">>Why does the right block " << Pattern->RightPred.BB->getName()
                     << " dominate any other blocks...?!?!\n");
        for (DomTreeNode *DTN : RightDTNChildren) {
          DT->changeImmediateDominator(DTN, NewTailDTN);
        }
      }
    }
  }
}

void Loopus::CanonicalizePredecessorsWorker::updateLoopInfo(struct InjectPattern *Pattern,
    BasicBlock *NewTail) {
  if (LI == nullptr) { return; }
  if ((Pattern == nullptr) || (NewTail == nullptr)) { return; }

  Loop *TailL = LI->getLoopFor(Pattern->TailBB);
  if (TailL == nullptr) { return; }
  TailL->addBasicBlockToLoop(NewTail, LI->getBase());
}

bool Loopus::CanonicalizePredecessorsWorker::injectDedicatedTail(struct InjectPattern *Pattern) {
  if (Pattern == nullptr) {
    return false;
  }

  // Test if an additional dedicated tail block is really needed. It is if the
  // current tail block has other predecessors than the two denoted in the
  // pattern struct.
  if (Loopus::getNumPredecessors(Pattern->TailBB) == 2) {
    return false;
  }
  if (Pattern->HeadBB != nullptr) {
    DEBUG(dbgs() << "Injecting tail for pattern at " << Pattern->HeadBB->getName() << ":\n");
  } else {
    DEBUG(dbgs() << "Injection for pattern with head=null requested!\n");
  }

  // Create a new block
  LLVMContext &GContext = getGlobalContext();
  BasicBlock *NewTailBB = BasicBlock::Create(GContext, "", Pattern->HeadBB->getParent());
  if (Pattern->HeadBB->hasName() == true) {
    NewTailBB->setName(Pattern->HeadBB->getName() + ".dedend");
  }
  // Now add the branch from the old to the new tail block
  BranchInst *NewTailBBBI = BranchInst::Create(Pattern->TailBB, NewTailBB);

  // Now adapt the phi nodes
  for (BasicBlock::iterator INSIT = Pattern->TailBB->begin(), INSEND = Pattern->TailBB->end();
      INSIT != INSEND; ++INSIT) {
    Instruction *I = &*INSIT;
    if (isa<PHINode>(I) == false) {
      // We reached a non-PHI instruction so we are done with all phi nodes
      break;
    }

    PHINode *OldPHI = dyn_cast<PHINode>(I);
    // Now we insert a phinode into the new tail block with the same values used
    // in the old block
    PHINode *NewBlockPHI = PHINode::Create(OldPHI->getType(), 2, "", NewTailBBBI);
    if (OldPHI->hasName() == true) {
      NewBlockPHI->setName(OldPHI->getName() + ".dedsink");
    }
    AAMDNodes TBAAMDNode;
    OldPHI->getAAMetadata(TBAAMDNode, false);
    NewBlockPHI->setAAMetadata(TBAAMDNode);
    NewBlockPHI->setDebugLoc(OldPHI->getDebugLoc());


    // Set the incoming values of the new PHI node
    Value *OldLeftPHIVal = OldPHI->getIncomingValueForBlock(Pattern->LeftPred.BB);
    Value *OldRightPHIVal = OldPHI->getIncomingValueForBlock(Pattern->RightPred.BB);
    NewBlockPHI->addIncoming(OldLeftPHIVal, Pattern->LeftPred.BB);
    NewBlockPHI->addIncoming(OldRightPHIVal, Pattern->RightPred.BB);
    // Now remove the incoming values for branch blocks from the old phi and
    // add the incoming value for the new tail block
    OldPHI->removeIncomingValue(Pattern->LeftPred.BB);
    OldPHI->removeIncomingValue(Pattern->RightPred.BB);
    OldPHI->addIncoming(NewBlockPHI, NewTailBB);
  }

  // Now remove the branches from the old branch blocks to the new dedicated tail
  // block
  Pattern->LeftPred.BB->getTerminator()->setSuccessor(Pattern->LeftPred.NumSucc, NewTailBB);
  Pattern->RightPred.BB->getTerminator()->setSuccessor(Pattern->RightPred.NumSucc, NewTailBB);

  updateDomTree(Pattern, NewTailBB);
  updateLoopInfo(Pattern, NewTailBB);

  ++StatsNumTailBlocksInjected;

  return true;
}

//===- Implementation of pass functions -----------------------------------===//
INITIALIZE_PASS_BEGIN(CanonicalizePredecessors, "loopus-canpreds", "Canonicalize pattern's predecessors", true, false)
INITIALIZE_PASS_END(CanonicalizePredecessors, "loopus-canpreds", "Canonicalize pattern's predecessors", true, false)

char CanonicalizePredecessors::ID = 0;

namespace llvm {
  Pass* createCanonicalizePredecessorsPass() {
    return new CanonicalizePredecessors();
  }
}

CanonicalizePredecessors::CanonicalizePredecessors(void)
 : FunctionPass(ID) {
  initializeCanonicalizePredecessorsPass(*PassRegistry::getPassRegistry());
}

void CanonicalizePredecessors::getAnalysisUsage(AnalysisUsage &AU) const {
}

bool CanonicalizePredecessors::runOnFunction(Function &F) {
  DominatorTreeWrapperPass *DTWP = getAnalysisIfAvailable<DominatorTreeWrapperPass>();
  DominatorTree *DT = nullptr;
  if (DTWP != nullptr) {
    DT = &DTWP->getDomTree();
  }

  Loopus::CanonicalizePredecessorsWorker CPW(DT, nullptr);
  return CPW.runOnFunction(&F);
}
