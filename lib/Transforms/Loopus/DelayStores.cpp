//===- DelayStores.cpp - Implementation of DelayStores pass ---------------===//
//
// Some parts were taken from the MergedLoadStoreMotion pass of the LLVM core.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "hdl-delaystores"

#include "DelayStores.h"

#include "LoopusUtils.h"

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Operator.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetLibraryInfo.h"

using namespace llvm;

STATISTIC(StatsNumHoistedLoads, "Number of loads hoisted to head.");
STATISTIC(StatsNumSunkStores, "Number of stores sunk into tail.");

//===- Implementation of herlper class ------------------------------------===//
Loopus::DelayStoresWorker::DelayStoresWorker(AliasAnalysis *AlAnal)
 : AA(AlAnal) {
}

bool Loopus::DelayStoresWorker::runOnBasicBlock(BasicBlock *BB) {
  if ((AA == nullptr)) { return false; }
  if (BB == nullptr) { return false; }

  bool changed = false;
  DelayPattern delaypattern(BB);
  if (isPatternHead(delaypattern) == true) {
    DEBUG(dbgs() << "   pat: head=" << delaypattern.HeadBB->getName() << "\n"
                 << "        bb0=" << ((delaypattern.BranchBB0 != nullptr) ? (delaypattern.BranchBB0->getName()) : ("null")) << "\n"
                 << "        bb1=" << ((delaypattern.BranchBB1 != nullptr) ? (delaypattern.BranchBB1->getName()) : ("null")) << "\n"
                 << "        tail=" << delaypattern.TailBB->getName() << "\n");
    changed |= mergeLoads(delaypattern);
    changed |= mergeStores(delaypattern);
  }

  return changed;
}

bool Loopus::DelayStoresWorker::runOnFunction(Function *F) {
  if ((AA == nullptr)) { return false; }
  if (F == nullptr) { return false; }

  bool changed = false;
  for (Function::iterator BBI = F->begin(), BBEND = F->end(); BBI != BBEND; ++BBI) {
    BasicBlock *BB = &*BBI;
    changed |= runOnBasicBlock(BB);
  }

  return changed;
}

/// \brief Determine if \c BB is the head of a diamond or triangle.
bool Loopus::DelayStoresWorker::isPatternHead(Loopus::DelayPattern &pattern) {
  if (pattern.HeadBB == nullptr) {
    pattern.Kind = DelayPattern::DelayPatternKind::Undefined;
    return false;
  }

  pattern.Kind = DelayPattern::DelayPatternKind::Undefined;
  BranchInst *BI = dyn_cast<BranchInst>(pattern.HeadBB->getTerminator());
  if (BI == nullptr) { return false; }
  if (BI->getNumSuccessors() != 2) { return false; }

  BasicBlock *Succ0 = BI->getSuccessor(0);
  BasicBlock *Succ1 = BI->getSuccessor(1);
  if ((Succ0 == nullptr) || (Succ1 == nullptr)) { return false; }

  TerminatorInst *Succ0TI = Succ0->getTerminator();
  if (Succ0TI == nullptr) { return false; }
  TerminatorInst *Succ1TI = Succ1->getTerminator();
  if (Succ1TI == nullptr) { return false; }

  const unsigned Succ0NumSuccs = Succ0TI->getNumSuccessors();
  const unsigned Succ1NumSuccs = Succ1TI->getNumSuccessors();
  // Now check the shape of the blocks. The branch blocks must have the head
  // as single predecessor. It would be possible to delay stores to the tail
  // if the branch block has several (more than 1) predecessors but in that case
  // if-conversion won't merge those blocks as it cannot determine the proper
  // condition for the select-instructions. Also this pass would have to insert
  // PHI-nodes for selecting the proper value to store and cannot use selects as
  // it also cannot choose the proper condition. So we ignore that case.
  if ((Succ0NumSuccs == 1) && (Succ1NumSuccs == 1)) {
    BasicBlock* const Succ0Succ = Succ0TI->getSuccessor(0);
    BasicBlock* const Succ1Succ = Succ1TI->getSuccessor(0);

    if (Succ0Succ == Succ1Succ) {
      if ((Succ0->getSinglePredecessor() == pattern.HeadBB)
       && (Succ1->getSinglePredecessor() == pattern.HeadBB)) {
        // This is a diamond
        pattern.BranchBB0 = Succ0;
        pattern.BranchBB1 = Succ1;
        pattern.TailBB = Succ0Succ;
        pattern.Kind = DelayPattern::DelayPatternKind::Diamond;
      }
    } else if (Succ0Succ == Succ1) {
      if (Succ0->getSinglePredecessor() == pattern.HeadBB) {
        // This is a triangle
        pattern.BranchBB0 = Succ0;
        pattern.BranchBB1 = nullptr;
        pattern.TailBB = Succ0Succ;
        pattern.Kind = DelayPattern::DelayPatternKind::Triangle;
      }
    } else if (Succ1Succ == Succ0) {
      if (Succ1->getSinglePredecessor() == pattern.HeadBB) {
        // This is a triangle
        pattern.BranchBB0 = Succ1;
        pattern.BranchBB1 = nullptr;
        pattern.TailBB = Succ1Succ;
        pattern.Kind = DelayPattern::DelayPatternKind::Triangle;
      }
    }
  } else if ((Succ0NumSuccs != 1) && (Succ1NumSuccs == 1)) {
    if (Succ1TI->getSuccessor(0) == Succ0) {
      if (Succ1->getSinglePredecessor() == pattern.HeadBB) {
        // This is a triangle
        pattern.BranchBB0 = Succ1;
        pattern.BranchBB1 = nullptr;
        pattern.TailBB = Succ0;
        pattern.Kind = DelayPattern::DelayPatternKind::Triangle;
      }
    }
  } else if ((Succ0NumSuccs == 1) && (Succ1NumSuccs != 1)) {
    if (Succ0TI->getSuccessor(0) == Succ1) {
      if (Succ0->getSinglePredecessor() == pattern.HeadBB) {
        // This is a triangle
        pattern.BranchBB0 = Succ0;
        pattern.BranchBB1 = nullptr;
        pattern.TailBB = Succ1;
        pattern.Kind = DelayPattern::DelayPatternKind::Triangle;
      }
    }
  } else {
    pattern.Kind = DelayPattern::DelayPatternKind::Undefined;
    return false;
  }

  if (pattern.Kind == DelayPattern::DelayPatternKind::Undefined) {
    return false;
  }

  // Determine if the tail has additional predecessor blocks than the two
  // branch blocks
  pred_iterator PPI = pred_begin(pattern.TailBB);
  pred_iterator PPIEND = pred_end(pattern.TailBB);
  if (pattern.Kind == DelayPattern::DelayPatternKind::Diamond) {
    unsigned numPreds = 0, numOtherPreds = 0;
    for ( ; PPI != PPIEND; ++PPI) {
      ++numPreds;
      if ((*PPI != pattern.BranchBB0) && (*PPI != pattern.BranchBB1)) {
        ++numOtherPreds;
      }
    }
    if (numOtherPreds != 0) {
      pattern.TailHasOtherPreds = true;
    }
  } else if (pattern.Kind == DelayPattern::DelayPatternKind::Triangle) {
    unsigned numPreds = 0, numOtherPreds = 0;
    for ( ; PPI != PPIEND; ++PPI) {
      ++numPreds;
      if ((*PPI != pattern.BranchBB0) && (*PPI != pattern.HeadBB)) {
        ++numOtherPreds;
      }
    }
    if (numOtherPreds != 0) {
      pattern.TailHasOtherPreds = true;
    }
  }

  if ((pattern.Kind == DelayPattern::DelayPatternKind::Triangle)
   || (pattern.Kind == DelayPattern::DelayPatternKind::Diamond)) {
    return true;
  } else {
    return false;
  }
}

bool Loopus::DelayStoresWorker::canInstructionRangeModRefHW(const Instruction &Start,
    const Instruction &End, const AliasAnalysis::Location &Loc,
    const AliasAnalysis::ModRefResult Mod) {
  if (Start.getParent() != End.getParent()) { return true; }

  BasicBlock::const_iterator INSIT = &Start;
  BasicBlock::const_iterator INSEND = &End;
  ++INSEND;

  for ( ; INSIT != INSEND; ++INSIT) {
    if (AA->getModRefInfo(INSIT, Loc) & Mod) {
      return true;
    }
  }

  return false;
}

/// \brief True if instruction range contains an instruction that a hoist barrier.
///
/// Whenever an instruction could possibly modify the value being loaded or
/// protect against the load from happening it is considered a hoist barrier.
bool Loopus::DelayStoresWorker::isLoadHoistBarrierInRange(const Instruction &Start,
    const Instruction &End, LoadInst *LI) {
  if (LI == nullptr) { return false; }
  AliasAnalysis::Location Loc = AA->getLocation(LI);
  return canInstructionRangeModRefHW(Start, End, Loc, AliasAnalysis::ModRefResult::Mod);
}

/// \brief True if instruction range contains an instruction that is a sink barrier.
///
/// Whenever an instruction could possibly read or modify the location being
/// stored to or protect against the store from happening it is considered a
/// sink barrier.
bool Loopus::DelayStoresWorker::isStoreSinkBarrierInRange(const Instruction &Start,
    const Instruction &End, AliasAnalysis::Location &LI) {
  return canInstructionRangeModRefHW(Start, End, LI, AliasAnalysis::ModRefResult::ModRef);
}

/// \brief Determine if the given instruction is safe to be hoisted elsewhere.
bool Loopus::DelayStoresWorker::isSafeToHoist(const Instruction *I) const {
  const BasicBlock *PBB = I->getParent();
  if (PBB == nullptr) { return false; }
  for (unsigned i = 0, e = I->getNumOperands(); i < e; ++i) {
    const Instruction *OPI = dyn_cast<Instruction>(I->getOperand(i));
    if ((OPI != nullptr) && (OPI->getParent() == PBB)) {
      return false;
    }
  }
  return true;
}

/// \brief Hoist an instruction into an other block.
Instruction* Loopus::DelayStoresWorker::hoistInstruction(BasicBlock *TBB, Instruction *SRCI) {
  if ((TBB == nullptr) || (SRCI == nullptr)) { return nullptr; }
  if (SRCI->getParent() == TBB) { return SRCI; }

  // Create new instruction
  Instruction *HoistedI = SRCI->clone();
  AA->copyValue(SRCI, HoistedI);
  AAMDNodes TBAAMDNode;
  SRCI->getAAMetadata(TBAAMDNode, false);
  HoistedI->setAAMetadata(TBAAMDNode);
  HoistedI->setDebugLoc(SRCI->getDebugLoc());
  if (SRCI->hasName() == true) {
    HoistedI->setName(SRCI->getName() + ".hoisted");
  }
  // Insert new instruction and remove old one
  HoistedI->insertBefore(TBB->getTerminator());
  SRCI->replaceAllUsesWith(HoistedI);
  SRCI->eraseFromParent();
  AA->deleteValue(SRCI);

  return HoistedI;
}

/// \brief Decide if a load can be hoisted
///
/// When there is a load in \c BB1 to the same address as \c LD0
/// and it can be hoisted from \c BB1, return that load.
/// Otherwise return \c null.
LoadInst* Loopus::DelayStoresWorker::canHoistFromBlock(BasicBlock *BB1, LoadInst *LD0) {
  if ((BB1 == nullptr) || (LD0 == nullptr)) { return nullptr; }
  BasicBlock *BB0 = LD0->getParent();
  if (BB0 == nullptr) { return nullptr; }

  for (BasicBlock::iterator INSIT = BB1->begin(), INSEND = BB1->end();
      INSIT != INSEND; ++INSIT) {
    LoadInst *LD1 = dyn_cast<LoadInst>(&*INSIT);
    if (LD1 == nullptr) { continue; }
    // PHI nodes are considered to evaluate their operands in the predecessor
    if (LD1->isUsedOutsideOfBlock(BB1) == true) { continue; }

    AliasAnalysis::Location Loc0 = AA->getLocation(LD0);
    AliasAnalysis::Location Loc1 = AA->getLocation(LD1);

    DEBUG(dbgs() << "  Considering load 1 : " << *LD1 << "\n");

    if ((AA->isMustAlias(Loc0, Loc1) == true)
     && (LD0->isSameOperationAs(LD1) == true)
     && (isLoadHoistBarrierInRange(BB0->front(), *LD0, LD0) == false)
     && (isLoadHoistBarrierInRange(BB1->front(), *LD1, LD1) == false)) {
      return LD1;
    }
  }
  return nullptr;
}

bool Loopus::DelayStoresWorker::hoistLoad(BasicBlock *HeadBB, LoadInst *LD0, LoadInst *LD1) {
  if ((LD0 == nullptr) || (LD1 == nullptr) || (HeadBB == nullptr)) { return false; }

  Instruction *PTR0 = dyn_cast<Instruction>(LD0->getPointerOperand());
  Instruction *PTR1 = dyn_cast<Instruction>(LD1->getPointerOperand());
  if ((PTR0 == nullptr) || (PTR1 == nullptr)) { return false; }

  // LLVM pass checks for PTR0->hasOneUse and PTR1->hasOneUse but I am not
  // really sure if that is needed
  if (PTR0 == PTR1) {
    // Both load instructions use the same variable as index to load from.
    // As the two load instructions are in two different branches, the index
    // must be computed in the head block or above (otherwise it could not be
    // available in both branches)
    if ((PTR0->getParent() == LD0->getParent())
     || (PTR0->getParent() == LD1->getParent())) { return false; }
    if (isSafeToHoist(LD0) == true) {
      DEBUG(dbgs() << "  hoist instruction into BB " << HeadBB->getName() << "\n"
                   << "  instruction left" << *LD0 << "\n"
                   << "  instruction right" << *LD1 << "\n");
      Instruction *HoistedLD = hoistInstruction(HeadBB, LD0);
      if (HoistedLD == nullptr) { return false; }
      LD1->replaceAllUsesWith(HoistedLD);
      LD1->eraseFromParent();
      AA->deleteValue(LD1);
      return true;
    }
  } else {
    if ((PTR0->getParent() == LD0->getParent())
     && (PTR1->getParent() == LD1->getParent())) {
      // Now we know: the index for each load is defined in the same block as
      // the load itself
      // This is the GEP instruction to hoist
      Instruction *LDPTR = nullptr;
      // This is the GEP instruction in the other block
      Instruction *OPTR = nullptr;
      // The load instruction to hoist
      LoadInst *HLD = nullptr;
      // The load instruction from the other block
      LoadInst *OLD = nullptr;

      // Test which GEP to use. As both GEPs are computing the same memory
      // location (else the memory locations of the loads would not be a
      // must-alias) it does not matter whoch GEP is hoisted
      if ((isa<GetElementPtrInst>(PTR0) == true)
       && (isSafeToHoist(PTR0) == true)) {
        // PTR0 can be hoisted
        LDPTR = PTR0; OPTR = PTR1;
        HLD = LD0; OLD = LD1;
      } else if ((isa<GetElementPtrInst>(PTR1) == true)
              && (isSafeToHoist(PTR1) == true)) {
        // PTR1 can be hoisted
        LDPTR = PTR1; OPTR = PTR0;
        HLD = LD1; OLD = LD0;
      } else {
        return false;
      }

      // Something went wrong... (should not happen)
      if ((LDPTR == nullptr) || (OPTR == nullptr) || (HLD == nullptr)
       || (OLD == nullptr)) { return false; }

      // If PTR0 and PTR1 are identical (so they are using the same operands,
      // types,...) we can later replace OPTR by LDPTR
      bool canReplaceOPTR = (OPTR->isIdenticalTo(LDPTR));

      DEBUG(dbgs() << "  hoist instruction into BB " << HeadBB->getName() << "\n"
                   << "  instruction one: " << *HLD << "\n"
                   << "  instruction two: " << *OLD << "\n");
      // Now hoist the GEP instruction
      Instruction *HoistedGEP = hoistInstruction(HeadBB, LDPTR);
      if (HoistedGEP == nullptr) { return false; }
      Instruction *HoistedLD = hoistInstruction(HeadBB, HLD);
      if (HoistedLD == nullptr) { return false; }

      // Replace LD1 by LD0
      OLD->replaceAllUsesWith(HoistedLD);
      OLD->eraseFromParent();
      AA->deleteValue(OLD);
      // Replace PTR1 by PTR0 if possible
      if (canReplaceOPTR == true) {
        OPTR->replaceAllUsesWith(HoistedGEP);
        Loopus::eraseFromParentRecursively(OPTR);
        AA->deleteValue(OPTR);
      } else if (OPTR->hasNUsesOrMore(1) == false) {
        Loopus::eraseFromParentRecursively(OPTR);
        AA->deleteValue(OPTR);
      }
      return true;
    }
  }

  return false;
}

/// \brief Try to hoist two loads to same address into diamond header
///
/// Starting from a diamond head block, iterate over the instructions in one
/// successor block and try to match a load in the second successor.
bool Loopus::DelayStoresWorker::mergeLoads(Loopus::DelayPattern &pattern) {
  // We do only merge loads within diamonds (The other cases can be handled by
  // the if-conversion pass itself if the appropriate option is set).
  if (pattern.Kind != DelayPattern::DelayPatternKind::Diamond) { return false; }
  if ((pattern.HeadBB == nullptr) || (pattern.BranchBB0 == nullptr)
   || (pattern.BranchBB1 == nullptr)) { return false; }
  // We ignore the tail here as we do not need it

  BranchInst *BI = dyn_cast<BranchInst>(pattern.HeadBB->getTerminator());
  if (BI == nullptr) { return false; }

  bool merged = false;
  for (BasicBlock::iterator INSIT = pattern.BranchBB0->begin(),
      INSEND = pattern.BranchBB0->end(); INSIT != INSEND; ) {

    // We need to do that workaround as the iterator might be invalidated when
    // a load is hoisted and thereby removed from a block
    Instruction *I = INSIT;
    ++INSIT;

    LoadInst *LD0 = dyn_cast<LoadInst>(I);
    if (LD0 == nullptr) { continue; }
    DEBUG(dbgs() << "can hoist? : " << *LD0 << "\n");

    if ((LD0->isSimple() == false)
     // PHI nodes are considered to evaluate their operands in the predecessor
     || (LD0->isUsedOutsideOfBlock(pattern.BranchBB0) == true)) {
      continue;
    }

    LoadInst *LD1 = canHoistFromBlock(pattern.BranchBB1, LD0);
    if (LD1 != nullptr) {
      DEBUG(dbgs() << "  found LD1  : " << *LD1 << "\n");
      // We found a matching load instruction to hoist
      bool hoistedld = hoistLoad(pattern.HeadBB, LD0, LD1);
      merged |= hoistedld;
      if (hoistedld == false) {
        // There were loads that matched but were not hoisted. Further loads
        // would be hoisted above those unhoisted loads. We should avoid that.
        break;
      } else {
        ++StatsNumHoistedLoads;
      }
    }
  }
  return merged;
}

/// \brief Checks if \c BB1 contains a store to the same location as \c ST0 does.
///
/// Returns a store instruction from \c BB1 that stores to the same memory
/// location as \c ST0 does and that can sunk out of the block.
StoreInst* Loopus::DelayStoresWorker::canSinkFromBlock(BasicBlock *BB1, StoreInst *ST0) {
  if ((BB1 == nullptr) || (ST0 == nullptr)) { return nullptr; }
  BasicBlock *BB0 = ST0->getParent();
  if (BB0 == nullptr) { return nullptr; }

  for (BasicBlock::reverse_iterator RINSIT = BB1->rbegin(), RINSEND = BB1->rend();
      RINSIT != RINSEND; ++RINSIT) {
    StoreInst *ST1 = dyn_cast<StoreInst>(&*RINSIT);
    if (ST1 == nullptr) { continue; }

    AliasAnalysis::Location Loc0 = AA->getLocation(ST0);
    AliasAnalysis::Location Loc1 = AA->getLocation(ST1);
    if ((AA->isMustAlias(Loc0, Loc1) == true)
     && (ST0->isSameOperationAs(ST1) == true)
     && (isStoreSinkBarrierInRange(*std::next(BasicBlock::iterator(ST0)),
        BB0->back(), Loc0 ) == false)
     && (isStoreSinkBarrierInRange(*std::next(BasicBlock::iterator(ST1)),
        BB1->back(), Loc1) == false)) {
      return ST1;
    }
  }

  return nullptr;
}

/// \brief Try to sink the two given stores into the tail block of the pattern
bool Loopus::DelayStoresWorker::sinkStore(BasicBlock *TailBB, StoreInst *ST0, StoreInst *ST1) {
  if ((ST0 == nullptr) || (ST1 == nullptr) || (TailBB == nullptr)) {
    return false;
  }

  Instruction *PTR0 = dyn_cast<Instruction>(ST0->getPointerOperand());
  Instruction *PTR1 = dyn_cast<Instruction>(ST1->getPointerOperand());
  if ((PTR0 == nullptr) || (PTR1 == nullptr)) { return false; }
  Value *VAL0 = ST0->getValueOperand();
  Value *VAL1 = ST1->getValueOperand();
  if ((VAL0 == nullptr) || (VAL1 == nullptr)) { return false; }

  // Determine insertion point of newly inserted instructions
  Instruction *InsertPt = TailBB->getFirstInsertionPt();
  if (InsertPt == nullptr) { return false; }

  if (PTR0 == PTR1) {
    // The stores are using the same GEP instruction. So we easily can sink them
    DEBUG(dbgs() << "  sink instruction into BB " << TailBB->getName() << "\n"
                 << "  instruction left  : " << *ST0 << "\n"
                 << "  instruction right : " << *ST1 << "\n");

    // Determine the value operand for the new store instruction
    Value *StoreVAL = nullptr;
    if (VAL0 == VAL1) {
      StoreVAL = VAL1;
    } else {
      PHINode *NewPN = PHINode::Create(VAL1->getType(), 2, "", TailBB->begin());
      if ((VAL0->hasName() == true) && (VAL1->hasName() == true)) {
        NewPN->setName(VAL0->getName() + "." + VAL1->getName() + ".sink");
      } else if ((VAL0->hasName() == false) && (VAL1->hasName() == true)) {
        NewPN->setName(VAL1->getName() + ".sink");
      } else if ((VAL0->hasName() == true) && (VAL1->hasName() == false)) {
        NewPN->setName(VAL0->getName() + ".sink");
      }
      NewPN->addIncoming(VAL0, ST0->getParent());
      NewPN->addIncoming(VAL1, ST1->getParent());
      StoreVAL = NewPN;
    }
    if (StoreVAL == nullptr) { return false; }

    StoreInst *NewST = dyn_cast<StoreInst>(ST1->clone());
    if ((ST0->hasName() == true) && (ST1->hasName() == true)) {
      NewST->setName(ST0->getName() + "." + ST1->getName() + ".sink");
    } else if ((ST0->hasName() == false) && (ST1->hasName() == true)) {
      NewST->setName(ST1->getName() + ".sink");
    } else if ((ST0->hasName() == true) && (ST1->hasName() == false)) {
      NewST->setName(ST0->getName() + ".sink");
    } else {
      NewST->setName("");
    }
    NewST->setOperand(0, StoreVAL);
    NewST->insertBefore(InsertPt);
    AAMDNodes TBAAMDNode;
    ST0->getAAMetadata(TBAAMDNode, false);
    ST1->getAAMetadata(TBAAMDNode, true);
    NewST->setAAMetadata(TBAAMDNode);
    AA->copyValue(ST0, NewST);
    NewST->setDebugLoc(ST0->getDebugLoc());

    Loopus::eraseFromParentRecursively(ST0);
    Loopus::eraseFromParentRecursively(ST1);
    AA->deleteValue(ST0);
    AA->deleteValue(ST1);
    return true;

  } else {
    if ((PTR0->getParent() == ST0->getParent())
     && (PTR1->getParent() == ST1->getParent())) {

      // Determine the value operand for the new store instruction
      Value *StoreVAL = nullptr;
      if (VAL0 == VAL1) {
        StoreVAL = VAL1;
      } else {
        PHINode *NewPN = PHINode::Create(VAL1->getType(), 2, "", TailBB->begin());
        if ((VAL0->hasName() == true) && (VAL1->hasName() == true)) {
          NewPN->setName(VAL0->getName() + "." + VAL1->getName() + ".sink");
        } else if ((VAL0->hasName() == false) && (VAL1->hasName() == true)) {
          NewPN->setName(VAL1->getName() + ".sink");
        } else if ((VAL0->hasName() == true) && (VAL1->hasName() == false)) {
          NewPN->setName(VAL0->getName() + ".sink");
        }
        NewPN->addIncoming(VAL0, ST0->getParent());
        NewPN->addIncoming(VAL1, ST1->getParent());
        StoreVAL = NewPN;
      }
      if (StoreVAL == nullptr) { return false; }

      // Now check if both instructions are GEP instructions
      GetElementPtrInst *GEP0 = dyn_cast<GetElementPtrInst>(PTR0);
      GetElementPtrInst *GEP1 = dyn_cast<GetElementPtrInst>(PTR1);
      if ((GEP0 == nullptr) || (GEP1 == nullptr)) { return false; }

      Value *MemLocVAL = nullptr;
      bool eraseGEPs = false;
      // Check if we can sink the GEP instruction too
      if ((GEP0->hasOneUse() == true) && (GEP1->hasOneUse() == true)) {
        // Both GEPs are only used in the associtated store
        if (GEP0->isIdenticalTo(GEP1) == true) {
          // Both GEPs are using exactly the same operands so they compute the
          // same result. So we clone one of them and move it into the tail.
          // Then both old GEPs are erased
          GetElementPtrInst *NewGEP = dyn_cast<GetElementPtrInst>(GEP1->clone());
          if ((GEP0->hasName() == true) && (GEP1->hasName() == true)) {
            NewGEP->setName(GEP0->getName() + "." + GEP1->getName() + ".sink");
          } else if ((GEP0->hasName() == false) && (GEP1->hasName() == true)) {
            NewGEP->setName(GEP1->getName() + ".sink");
          } else if ((GEP0->hasName() == true) && (GEP1->hasName() == false)) {
            NewGEP->setName(GEP0->getName() + ".sink");
          } else {
            NewGEP->setName("");
          }
          AAMDNodes TBAAMDNode;
          GEP1->getAAMetadata(TBAAMDNode, false);
          NewGEP->setAAMetadata(TBAAMDNode);
          AA->copyValue(GEP1, NewGEP);
          NewGEP->setDebugLoc(GEP1->getDebugLoc());
          NewGEP->insertBefore(InsertPt);
          // Erase old GEPs
          eraseGEPs = true;
          MemLocVAL = NewGEP;
        #if 0
        } else if (GEP0->isSameOperationAs(GEP1) == true) {
          // They have the same number of operands but do not use the same
          // operands. So we need a PHI for each operand that is different and
          // create a new GEP in the tail.
          SmallVector<Value*, 8> NewGEPOps;
          for (unsigned i = 0, e = GEP1->getNumOperands(); i < e; ++i) {
            Value* NGOP0 = GEP0->getOperand(i);
            Value* NGOP1 = GEP1->getOperand(i);
            if ((NGOP0 == nullptr) || (NGOP1 == nullptr)) { return false; }
            if (NGOP0 == NGOP1) {
              // Both GEPs are using the same operand at the same position so
              // no PHI is needed for this operand
              NewGEPOps.push_back(NGOP1);
            } else {
              // The GEPs are using different operands at a certain position. So
              // we must create PHI node for this operand position and use this
              // PHI nodes result in the GEP. Because the GEPs are same at least
              // the types of the operands are matching
              PHINode *NewPN = PHINode::Create(NGOP1->getType(), 2,
                NGOP1->getName() + ".sink", TailBB->begin());
              NewPN->addIncoming(NGOP0, ST0->getParent());
              NewPN->addIncoming(NGOP1, ST1->getParent());
              NewGEPOps.push_back(NewPN);
            }
          }
          if (NewGEPOps.size() != GEP1->getNumOperands()) { return false; }

          GetElementPtrInst *NewGEP = dyn_cast<GetElementPtrInst>(GEP1->clone());
          if (GEP1->hasName() == true) {
            NewGEP->setName(GEP1->getName() + ".cpy");
          }
          for (unsigned i = 0, e = NewGEPOps.size(); i < e; ++i) {
            NewGEP->setOperand(i, NewGEPOps[i]);
          }
          NewGEP->insertBefore(InsertPt);
          // Erase old GEPs
          eraseGEPs = true;
          MemLocVAL = NewGEP;
        #endif
        } else {
          // The GEPs are using different operand lists so we cannot simply
          // create a new one in the tail. Instead use a PHI node in the tail
          // for merging the results of the GEPs
          PHINode *NewPN = PHINode::Create(GEP1->getType(), 2,
            GEP1->getName() + ".sink", TailBB->begin());
          NewPN->addIncoming(GEP0, ST0->getParent());
          NewPN->addIncoming(GEP1, ST1->getParent());
          MemLocVAL = NewPN;
        }
      } else {
        // At least one of the GEP instructions is used several times so we cannot
        // sink it and we will create a PHI node in the tail for the result of
        // the GEPs instead
        PHINode *NewPN = PHINode::Create(GEP1->getType(), 2,
          GEP1->getName() + ".sink", TailBB->begin());
        NewPN->addIncoming(GEP0, ST0->getParent());
        NewPN->addIncoming(GEP1, ST1->getParent());
        MemLocVAL = NewPN;
      }
      if (MemLocVAL == nullptr) { return false; }

      StoreInst *NewST = dyn_cast<StoreInst>(ST1->clone());
      if ((ST0->hasName() == true) && (ST1->hasName() == true)) {
        NewST->setName(ST0->getName() + "." + ST1->getName() + ".sink");
      } else if ((ST0->hasName() == false) && (ST1->hasName() == true)) {
        NewST->setName(ST1->getName() + ".sink");
      } else if ((ST0->hasName() == true) && (ST1->hasName() == false)) {
        NewST->setName(ST0->getName() + ".sink");
      } else {
        NewST->setName("");
      }
      AAMDNodes TBAAMDNode;
      ST0->getAAMetadata(TBAAMDNode, false);
      ST1->getAAMetadata(TBAAMDNode, true);
      NewST->setAAMetadata(TBAAMDNode);
      AA->copyValue(ST1, NewST);
      NewST->setDebugLoc(ST1->getDebugLoc());

      NewST->setOperand(0, StoreVAL);
      NewST->setOperand(1, MemLocVAL);
      NewST->insertBefore(InsertPt);
      ST0->eraseFromParent();
      ST1->eraseFromParent();
      AA->deleteValue(ST0);
      AA->deleteValue(ST1);
      if (eraseGEPs == true) {
        Loopus::eraseFromParentRecursively(GEP0);
        Loopus::eraseFromParentRecursively(GEP1);
        AA->deleteValue(GEP0);
        AA->deleteValue(GEP1);
      }
      return true;
    }
  }

  return false;
}

bool Loopus::DelayStoresWorker::mergeStores(Loopus::DelayPattern &pattern) {
  if ((pattern.Kind != DelayPattern::DelayPatternKind::Diamond)
   && (pattern.Kind != DelayPattern::DelayPatternKind::Triangle)) { return false; }
  if (pattern.TailHasOtherPreds == true) { return false; }

  BasicBlock *OBB = nullptr;
  if (pattern.Kind == DelayPattern::DelayPatternKind::Diamond) {
    OBB = pattern.BranchBB1;
  } else if (pattern.Kind == DelayPattern::DelayPatternKind::Triangle) {
    OBB = pattern.HeadBB;
  }
  if (OBB == nullptr) { return false; }

  bool sunk = false;
  for (BasicBlock::reverse_iterator RINSIT = pattern.BranchBB0->rbegin(),
      RINSEND = pattern.BranchBB0->rend(); RINSIT != RINSEND; ) {

    // As removing a store might invalidate the iterators
    Instruction *I = &*RINSIT;
    ++RINSIT;

    StoreInst *ST0 = dyn_cast<StoreInst>(I);
    if (ST0 == nullptr) { continue; }
    DEBUG(dbgs() << "can sink?  : " << *ST0 << "\n");

    if (ST0->isSimple() == false) { continue; }

    // Search for similiar store in other basic block
    StoreInst *ST1 = canSinkFromBlock(OBB, ST0);
    if ((ST1 != nullptr) && (pattern.Kind == DelayPattern::DelayPatternKind::Triangle)) {
      // If we found a possible matching store instruction in the head of a
      // triangle we must still test the range [Block->begin(),ST0) in the
      // *BRANCH* block for a store barrier
      AliasAnalysis::Location ST1Loc = AA->getLocation(ST1);
      if (isStoreSinkBarrierInRange(pattern.BranchBB0->front(),
          *std::prev(BasicBlock::iterator(ST0)), ST1Loc) == true) {
        ST1 = nullptr;
      }
    }
    if (ST1 != nullptr) {
      DEBUG(dbgs() << "  found ST1  : " << *ST1 << "\n");
      // We found a matching store instruction to sink
      bool sunkst = sinkStore(pattern.TailBB, ST0, ST1);
      sunk |= sunkst;
      if (sunkst == false) {
        break;
      } else {
        ++StatsNumSunkStores;
        RINSIT = pattern.BranchBB0->rbegin();
        RINSEND = pattern.BranchBB0->rend();
        DEBUG(dbgs() << "  Starting over...\n");
      }
    }
  }

  return sunk;
}

//===- Implementation of LLVM pass ----------------------------------------===//
INITIALIZE_PASS_BEGIN(DelayStores, "loopus-delst", "Delay store instructions",  true, false)
INITIALIZE_AG_DEPENDENCY(AliasAnalysis)
INITIALIZE_PASS_DEPENDENCY(TargetLibraryInfo)
INITIALIZE_PASS_END(DelayStores, "loopus-delst", "Delay store instructions",  true, false)

char DelayStores::ID = 0;

namespace llvm {
  Pass* createDelayStoresPass() {
    return new DelayStores();
  }
}

DelayStores::DelayStores(void)
 : FunctionPass(ID) {
  initializeDelayStoresPass(*PassRegistry::getPassRegistry());
}

void DelayStores::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<AliasAnalysis>();
  AU.addRequired<TargetLibraryInfo>();
  AU.addPreserved<TargetLibraryInfo>();
  AU.setPreservesCFG();
}

bool DelayStores::runOnFunction(Function &F) {
  AliasAnalysis *AA = &getAnalysis<AliasAnalysis>();

  if (AA == nullptr) {
    return false;
  }

  Loopus::DelayStoresWorker DSW(AA);
  return DSW.runOnFunction(&F);
}
