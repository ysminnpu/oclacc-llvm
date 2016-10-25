//===- AggregateLoads.cpp -------------------------------------------------===//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "loopus-aggregateloads"

#include "AggregateLoads.h"

#include "HardwareModel.h"
#include "LoopusUtils.h"

#include "llvm/ADT/Twine.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/raw_ostream.h"

#include <algorithm>

using namespace llvm;

STATISTIC(StatsNumAggregatedLoads, "Number of aggregated loads.");
STATISTIC(StatsNumCreatedLoads, "Number of new created loads.");

//===- Helper function ----------------------------------------------------===//
void printAPInt(const APInt &value, raw_ostream &stream, unsigned width, unsigned radix = 10) {
  SmallVector<char, 32> IntString;
  value.toStringUnsigned(IntString, radix);
  if (radix == 2) {
    stream << "0b";
  } else if (radix == 8) {
    stream << "0";
  } else if (radix == 16) {
    stream << "0x";
  }
  int LeadingZeros = width - IntString.size();
  unsigned StrLen = IntString.size();

  for (int i = 0; i < LeadingZeros; ++i) {
    stream << "0";
  }
  for (unsigned i = 0; i < StrLen; ++i) {
    stream << IntString[i];
  }
}

//===- Functions for transformations --------------------------------------===//

/// Tries to find a matching load for the given load instruction. A matching
/// load is a load instruction that reads from a memory position directly
/// before or after the given one. So for a load that loads from array[i] a
/// matching load would be array[i-1] or array[i+1]. If the found load reads
/// from a position after the given load (from a "higher" address) the parameter
/// \c areReversed is set to \c true.
LoadInst* AggregateLoads::findMatchingLoad(LoadInst* const LD, bool &areReversed,
    const SmallPtrSetImpl<LoadInst*> *AvoidanceList) {
  if (LD == nullptr) { return nullptr; }

  const AliasAnalysis::Location LDLoc = AA->getLocation(LD);
  BasicBlock* const LDBB = LD->getParent();
  Loop* const LDParentLoop = LLI->getLoopFor(LDBB);

  // This are needed to process loads inside a loop properly
  Value *LDGEP = LD->getPointerOperand();
  if (LDGEP == nullptr) { return nullptr; }
  const SCEV *LDSCEVGeneral = SE->getSCEV(LDGEP);
  if (LDSCEVGeneral == nullptr) { return nullptr; }
  const SCEV *LDSCEVOffset = nullptr;
  const SCEV *LDSCEVStride = nullptr;
  if (isa<SCEVCouldNotCompute>(LDSCEVGeneral) == true) {
    return nullptr;
  } else if (isa<SCEVAddRecExpr>(LDSCEVGeneral) == true) {
    const SCEVAddRecExpr *LDSCEVAddR = dyn_cast<SCEVAddRecExpr>(LDSCEVGeneral);
    LDSCEVOffset = LDSCEVAddR->getOperand(0);
    LDSCEVStride = LDSCEVAddR->getOperand(1);
  } else {
    LDSCEVOffset = LDSCEVGeneral;
    LDSCEVStride = SE->getConstant(LDSCEVOffset->getType(), 0, false);
  }
  if ((LDSCEVOffset == nullptr) || (LDSCEVStride == nullptr)) {
    return nullptr;
  }

  // Now build a list of all blocks that we might want to visit
  SmallVector<BasicBlock*, 8> VisitBlocks;
  BasicBlock *curBB = LDBB;
  while (curBB != nullptr) {
    Loop* const curLoop = LLI->getLoopFor(curBB);
    if (curLoop == LDParentLoop) {
      VisitBlocks.push_back(curBB);
    }
    // We cannot use the immdom here:
    //      BB H
    //     /    \
    // BB T      BB F
    //     \    /
    //      BB T
    // If both T and F write to a memory location that is loaded in H then we
    // would have to detect this but that would be quite difficult to find out.
    curBB = curBB->getUniquePredecessor();
  }

  // This vector stores all blocks that we visited for finding a matching load.
  // Those blocks must later be checked for store barriers if a matching load
  // was found.
  SmallVector<BasicBlock*, 8> VisitedBlocks;
  VisitedBlocks.push_back(LDBB);

  // Some things needed in the loop and that do not need to be recomputed in
  // every iteration
  const BasicBlock::iterator BBIT(LD);
  BasicBlock::reverse_iterator BBRITPreLd(std::next(BBIT));

  for (SmallVector<BasicBlock*, 8>::iterator BBIT = VisitBlocks.begin(),
      BBEND = VisitBlocks.end(); BBIT != BBEND; ++BBIT) {
    BasicBlock *curBB = *BBIT;
    DEBUG(dbgs() << "Inspecting block " << curBB->getName() << "\n");

    BasicBlock::reverse_iterator BBRFIRST = curBB->rbegin();
    if (curBB == LDBB) {
      BBRFIRST = BasicBlock::reverse_iterator(LD);
    }
    for (BasicBlock::reverse_iterator BBRIT = BBRFIRST, BBREND = curBB->rend();
        BBRIT != BBREND; ++BBRIT) {
      Instruction *CurI = &*BBRIT;

      if (isa<StoreInst>(CurI) == true) {
        // If the store writes to a location where we want to load from we bail
        // out...
        const StoreInst *CurSI = dyn_cast<StoreInst>(CurI);
        const AliasAnalysis::Location CurSILoc = AA->getLocation(CurSI);
        if (AA->isNoAlias(LDLoc, CurSILoc) == false) {
          // This store seems to be a barrier so that's it...
          DEBUG(dbgs() << "  Found STORE barrier: " << *CurSI << " @" << CurSI << "\n");
          return nullptr;
        }
      } else if (isa<CallInst>(CurI) == true) {
        // We need to check for a barrier function call
        const CallInst *CurCI = dyn_cast<CallInst>(CurI);
        const Function *CalledF = CurCI->getCalledFunction();
        if (CalledF == nullptr) {
          continue;
        }
        if (MFN->isSynchronizationFunction(CalledF->getName()) == true) {
          DEBUG(dbgs() << "  Found SYNC barrier: " << *CurCI << " @" << CurCI << "\n");
          return nullptr;
        }
      }
      if ((AA->getModRefInfo(CurI, LDLoc) & AliasAnalysis::ModRefResult::Mod) != 0) {
        DEBUG(dbgs() << "  Found BARRIER: " << *CurI << " @" << CurI << "\n");
        return nullptr;
      }

      LoadInst *CurLD = dyn_cast<LoadInst>(CurI);
      if (CurLD == nullptr) {
        // This is not a valid load instruction
        continue;
      }

      DEBUG(dbgs() << "  Testing Load: " << *CurLD << " @" << CurLD << "\n");
      // Some simple tests
      if (CurLD->isSimple() == false) { continue; }
      if ((CurLD->getType()->isIntegerTy() == false)
       && (CurLD->getType()->isFloatingPointTy() == false)) { continue; }
      if (LD->getPointerAddressSpace() != CurLD->getPointerAddressSpace()) {
        continue;
      }

      // Try to proofe that there is no gap between the loads
      Value *CurLDGEP = CurLD->getPointerOperand();
      if (CurLDGEP == nullptr) { continue; }
      const SCEV *CurLDSCEVGeneral = SE->getSCEV(CurLDGEP);
      DEBUG(dbgs() << "  scev: cld=" << *CurLDSCEVGeneral << "\n");
      if (CurLDSCEVGeneral == nullptr) { continue; }
      const SCEV *CurLDSCEVOffset = nullptr;
      const SCEV *CurLDSCEVStride = nullptr;
      if (isa<SCEVCouldNotCompute>(CurLDSCEVGeneral) == true) {
        continue;
      } else if (isa<SCEVAddRecExpr>(CurLDSCEVGeneral) == true) {
        const SCEVAddRecExpr *CurLDSCEVAddR = dyn_cast<SCEVAddRecExpr>(CurLDSCEVGeneral);
        CurLDSCEVOffset = CurLDSCEVAddR->getOperand(0);
        CurLDSCEVStride = CurLDSCEVAddR->getOperand(1);
      } else {
        CurLDSCEVOffset = CurLDSCEVGeneral;
        CurLDSCEVStride = SE->getConstant(CurLDSCEVOffset->getType(), 0, false);
      }
      if ((CurLDSCEVOffset == nullptr) || (CurLDSCEVStride == nullptr)) {
        continue;
      }

      DEBUG(dbgs() << "  scev: ld=" << *LDSCEVGeneral << "\n"
                   << "        cld=" << *CurLDSCEVGeneral << "\n");

      // Test if the strides are same. If not then the gap between the loads will
      // get greater from itertation to iteration and therefore the bitmask would
      // depend on the iteration count.
      const SCEV *StrideDiff = SE->getMinusSCEV(LDSCEVStride, CurLDSCEVStride);
      if (StrideDiff->isZero() == false) {
        DEBUG(dbgs() << "  Rejected load as strides of loads differ!\n");
        continue;
      }

      // Compute difference between the two loads
      const SCEV *MemAddrDiff = SE->getMinusSCEV(LDSCEVOffset, CurLDSCEVOffset);
      DEBUG(dbgs() << "  mem:  diff=" << *MemAddrDiff << " @" << MemAddrDiff << "\n");
      // Determine if the gap is zero
      const SCEV *ElemAllignSz = SE->getSizeOfExpr(MemAddrDiff->getType(), CurLD->getType());
      const SCEV *ElemDiff = SE->getMinusSCEV(MemAddrDiff, ElemAllignSz);
      DEBUG(dbgs() << "  elem: sz=" << *ElemAllignSz << " @" << ElemAllignSz << "\n"
          << "        diff=" << *ElemDiff << " @" << ElemDiff << "\n");

      bool areReversedLoads = false;
      if (ElemDiff->isZero() == true) {
        // The loads are in proper order: first the low address and later the high
        // address
        areReversedLoads = false;
      } else {
        // Check if the loads are in reverse order
        const SCEV *ElemAllignSzRev = SE->getSizeOfExpr(MemAddrDiff->getType(), LD->getType());
        // We must use an add here as the MemAddrDiff holds the negative address
        // difference if they are in reverse order
        const SCEV *ElemDiffRev = SE->getAddExpr(MemAddrDiff, ElemAllignSzRev);
        DEBUG(dbgs() << "        revsz=" << *ElemAllignSzRev << " @" << ElemAllignSzRev << "\n"
            << "        revdiff=" << *ElemDiffRev << " @" << ElemDiffRev << "\n");
        if (ElemDiffRev->isZero() == true) {
          areReversedLoads = true;
        } else {
          areReversedLoads = false;
          DEBUG(dbgs() << "  Rejected load: gap is greater than zero!\n");
          continue;
        }
      }
      // So now we know:
      // |---------|----------------|---------|
      //    Load0       GapBytes       Load1
      //    CurLD         (=0)          LD
      // ( The load from the lower address (CurLD) is executed BEFORE the load  )
      // ( from the higher address (LD).                                        )
      //
      // or
      // |---------|----------------|---------|
      //    Load1       GapBytes       Load0
      //     LD           (=0)         CurLD
      // ( The load from the lower address (LD) is executed AFTER the load      )
      // ( from the higher address (CurLD).                                     )

      // Now make sure that there is no store that overwrites the location loaded
      // from by the preceding load (CurLD) in all visited basic blocks (no
      // matter from it loads its value from).
      #if 0
      bool foundStoreBarrier = false;
      const AliasAnalysis::Location CurLDLoc = AA->getLocation(CurLD);
      for (SmallVector<BasicBlock*, 8>::iterator ChkBBIT = VisitBlocks.begin(),
          ChkBBEND = std::next(BBIT); ChkBBIT != ChkBBEND; ++ChkBBIT) {
        const BasicBlock *CheckBB = *ChkBBIT;
        BasicBlock::const_iterator CHKIT = CheckBB->begin();
        BasicBlock::const_iterator CHKEND = CheckBB->end();
        if (CheckBB == CurLD->getParent()) {
          // Currently inspecting parent block of CurLD so CurLD is the first
          // instruction to check
          CHKIT = BasicBlock::const_iterator(CurLD);
        }
        if (CheckBB == LDBB) {
          // Now checking parent block of LD. So LD is the first instruction not
          // to check
          CHKEND = BasicBlock::const_iterator(LD);
        }
        for ( ; CHKIT != CHKEND; ++CHKIT) {
          const Instruction *CurChkI = &*CHKIT;
          if (isa<StoreInst>(CurChkI) == false) { continue; }
          const StoreInst *CurChkST = dyn_cast<StoreInst>(CurChkI);
          const AliasAnalysis::Location CurChkSTLoc = AA->getLocation(CurChkST);
          if (AA->isNoAlias(CurLDLoc, CurChkSTLoc) == false) {
            foundStoreBarrier = true;
          }
        }
      }
      if (foundStoreBarrier == true) { continue; }
      #endif

      areReversed = areReversedLoads;
      return CurLD;
    } // End of instruction reverse-iterator

  } // End of basic block iterator

  return nullptr;
}

/// For each load instruction an entry in the map is created and a pointer that
/// points to an other load is inserted. That other load reads from a memory
/// location directly before (at a "lower" address) the memory position of the
/// key load.
void AggregateLoads::populateLoadChain(Function &F, LoadChainMapTy &LoadChain) {
  for (inst_iterator INSIT = inst_begin(F), INSEND = inst_end(F);
      INSIT != INSEND; ++INSIT) {
    Instruction *I = &*INSIT;
    if (isa<LoadInst>(I) == false) { continue; }
    LoadInst *LD = dyn_cast<LoadInst>(I);
    DEBUG(dbgs() << "Found LOAD: " << *LD << " @" << LD << "\n");

    SmallPtrSet<LoadInst*, 16> AvoidanceList;
    bool areReversedLDs = false;
    LoadInst *OtherLD = findMatchingLoad(LD, areReversedLDs, &AvoidanceList);
    if (OtherLD == nullptr) {
      // No matching load found so check next instruction
      continue;
    }

    LoadInst *LoLD = nullptr; LoadInst *HiLD = nullptr;
    if (areReversedLDs == false) {
      LoLD = OtherLD; HiLD = LD;
    } else {
      LoLD = LD; HiLD = OtherLD;
    }

    AvoidanceList.insert(LoLD);
    LoadChain[HiLD] = LoLD;
  }
}

/// Computes the load chain to which tge given load belongs to and returns the
/// length (number of loads) of that chain. The loads themself are inserted at
/// the end of the provided vector starting with the load from the highest
/// address.
unsigned AggregateLoads::computeLoadChain(LoadChainMapTy &LoadChain,
    LoadInst* const LD, SmallVectorImpl<LoadInst*> &LDChain) {
  if (LD == nullptr) { return 0; }
  if (LoadChain.count(LD) == 0) { return 0; }

  // First we have to go to the end of the load chain
  LoadInst *CurFwdLD = LD;
  bool hasSuccessorLD = true;
  do {
    hasSuccessorLD = false;
    for (LoadChainMapTy::iterator CLIT = LoadChain.begin(), CLEND = LoadChain.end();
        CLIT != CLEND; ++CLIT) {
      if (CLIT->second == CurFwdLD) {
        CurFwdLD = CLIT->first;
        hasSuccessorLD = true;
        break;
      }
    }
  } while (hasSuccessorLD == true);

  unsigned ChainSz = 0;
  LoadInst *CurLD = CurFwdLD;
  while (CurLD != nullptr) {
    // The current load belongs to the load chain
    LDChain.push_back(CurLD);
    ++ChainSz;
    // Now go to the next load if existing
    if (LoadChain.count(CurLD) == 0) {
      CurLD = nullptr;
    } else {
      CurLD = LoadChain[CurLD];
    }
  }

  return ChainSz;
}

/// Splits one given LoadChain into several chunks where each chunk loads at
/// most as many bytes as the memory interface provides.
unsigned AggregateLoads::splitIntoLoadChainChunks(SmallVectorImpl<LoadInst*> &LDChain,
    SmallVectorImpl<LoadChainChunkTy> &LDChunks) {
  // Number of created chunks
  unsigned numChunks = 1;
  // The width of the global memory bus
  const unsigned GMemBusWidth = Loopus::HardwareModel::getHardwareModel().getGMemBusWidthInBytes();

  // Current bitwidth for a load for the given chunk
  uint64_t curChunkSz = 0;
  // Loads of the current chunk
  LoadChainChunkTy curChunk;
  // The load that is dominated by all other loads within the current chunk
  LoadInst *leastDominatingLD = nullptr;
  LoadInst *mostDominatingLD = nullptr;

  unsigned nextChainElemIdx = 0, lastChainElemIdx = LDChain.size();
  while (nextChainElemIdx < lastChainElemIdx) {
    LoadInst *curElem = LDChain[nextChainElemIdx];
    const uint64_t curElemSz = DL->getTypeAllocSize(curElem->getType());
    
    // Indicates if the current load should start a new chunk
    bool startNewChunk = false;
    // Determine the load that is dominated by all other loads already in the chunk
    if (leastDominatingLD != nullptr) {
      if (DT->dominates(curElem, leastDominatingLD) == false) {
        leastDominatingLD = curElem;
      }
    } else {
      leastDominatingLD = curElem;
    }
    // Determine the load that dominates all other loads in the chunk
    if (mostDominatingLD != nullptr) {
      if (DT->dominates(curElem, mostDominatingLD) == true) {
        mostDominatingLD = curElem;
      }
    } else {
      mostDominatingLD = curElem;
    }
    // Some consistency
    DEBUG(
      DEBUG(dbgs() << "Domination relation in chunk:\n"
                   << "  doms: mdom=" << *mostDominatingLD << " @" << mostDominatingLD << "\n"
                   << "        ldom=" << *leastDominatingLD << " @" << leastDominatingLD << "\n");
      for (const LoadInst *testLD : curChunk) {
        if (testLD != leastDominatingLD) {
          if (DT->dominates(testLD, leastDominatingLD) == false) {
            llvm_unreachable("Not-least-dominating load does NOT dominate least-dominating load!");
          }
        }
        if (testLD != mostDominatingLD) {
          if (DT->dominates(mostDominatingLD, testLD) == false) {
            llvm_unreachable("Most-dominating load does NOT dominate not-most-dominating load!");
          }
        }
      }
      if (curElem != leastDominatingLD) {
        if (DT->dominates(curElem, leastDominatingLD) == false) {
          llvm_unreachable("Not-least-dominating load (curElem) does NOT dominate least-dominating load!");
        }
      }
      if (curElem != mostDominatingLD) {
        if (DT->dominates(mostDominatingLD, curElem) == false) {
          llvm_unreachable("Most-dominating load does NOT dominate not-most--dominating load (curElem)!");
        }
      }
    );
    // Now check if there is any store barrier between the least and most
    // dominating load
    const BasicBlock *curBB = leastDominatingLD->getParent();
    while (true) {
      BasicBlock::const_iterator IIT = curBB->begin(), IEND = curBB->end();
      if (curBB == mostDominatingLD->getParent()) {
        IIT = BasicBlock::iterator(mostDominatingLD);
      }
      if (curBB == leastDominatingLD->getParent()) {
        IEND = BasicBlock::iterator(leastDominatingLD);
      }
      // Test all contained instructions
      for ( ; IIT != IEND; ++IIT) {
        const Instruction *CurI = &*IIT;
        if (isa<StoreInst>(CurI) == true) {
          // Test if the store writes to a location that should be used within
          // the current chunk
          const StoreInst *CurSI = dyn_cast<StoreInst>(CurI);
          const AliasAnalysis::Location CurSILoc = AA->getLocation(CurSI);
          for (const LoadInst *TestLD : curChunk) {
            const AliasAnalysis::Location TestLDLoc = AA->getLocation(TestLD);
            if (AA->isNoAlias(TestLDLoc, CurSILoc) == false) {
              DEBUG(dbgs() << "  Found STORE barrier: " << *CurSI << " @" << CurSI << "\n"
                           << "  >>Rejecting current element fur current chunk: " << *curElem << " @" << curElem << "\n"
                           << "  >>Creating new chunk!\n");
              startNewChunk = true;
            }
          }
          const AliasAnalysis::Location curElemLoc = AA->getLocation(curElem);
          if (AA->isNoAlias(curElemLoc, CurSILoc) == false) {
            DEBUG(dbgs() << "  Found STORE barrier: " << *CurSI << " @" << CurSI << "\n"
                         << "  >>Rejecting current element fur current chunk: " << *curElem << " @" << curElem << "\n"
                << "  >>Creating new chunk!\n");
            startNewChunk = true;
          }
        } else if (isa<CallInst>(CurI) == true) {
          const CallInst *CurCI = dyn_cast<CallInst>(CurI);
          const Function *CalledF = CurCI->getCalledFunction();
          if ((CalledF != nullptr)
           && (MFN->isSynchronizationFunction(CalledF->getName()) == true)) {
              DEBUG(dbgs() << "  Found SYNC barrier: " << *CurCI << " @" << CurCI << "\n"
                           << "  >>Rejecting current element fur current chunk: " << *curElem << " @" << curElem << "\n"
                           << "  >>Creating new chunk!\n");
              startNewChunk = true;
          }
        }
        // Some general tests
        for (const LoadInst *TestLD : curChunk) {
          const AliasAnalysis::Location TestLDLoc = AA->getLocation(TestLD);
          if ((AA->getModRefInfo(CurI, TestLDLoc) & AliasAnalysis::ModRefResult::Mod) != 0) {
            DEBUG(dbgs() << "  Found BARRIER: " << *CurI << " @" << CurI << "\n"
                         << "  >>Rejecting current element fur current chunk: " << *curElem << " @" << curElem << "\n"
                         << "  >>Creating new chunk!\n");
            startNewChunk = true;
          }
        }
        const AliasAnalysis::Location curElemLoc = AA->getLocation(curElem);
        if ((AA->getModRefInfo(CurI, curElemLoc) & AliasAnalysis::ModRefResult::Mod) != 0) {
          DEBUG(dbgs() << "  Found BARRIER: " << *CurI << " @" << CurI << "\n"
                       << "  >>Rejecting current element fur current chunk: " << *curElem << " @" << curElem << "\n"
              << "  >>Creating new chunk!\n");
          startNewChunk = true;
        }
      }
      // Go to next preceeding block
      if (curBB != mostDominatingLD->getParent()) {
        const BasicBlock *nextBB = curBB->getUniquePredecessor();
        if (nextBB == nullptr) {
          llvm_unreachable("Current block does not have a unique predecessor!");
        }
        curBB = nextBB;
      } else {
        break;
      }
    }   // End of while-loop for testing for sync, call and store barriers

    // Test for bus width
    if (curChunkSz + curElemSz > GMemBusWidth) {
      startNewChunk = true;
    }

    if (startNewChunk == false) {
      // Add current load to the current chunk
      curChunk.push_back(curElem);
      curChunkSz = curChunkSz + curElemSz;
    } else {
      // Current element should start new chunk. So finish current chunk and add
      // it to list of chunks.
      LDChunks.push_back(curChunk);
      curChunk.clear();
      // Start new chunk
      curChunk.push_back(curElem);
      curChunkSz = curElemSz;
      leastDominatingLD = curElem;
      mostDominatingLD = curElem;
      ++numChunks;
    }
    ++nextChainElemIdx;
  }
  // Now add the last chunk as it is not yet added to the chunk list
  LDChunks.push_back(curChunk);

  return numChunks;
}

bool AggregateLoads::handleChunk(SmallVectorImpl<LoadInst*> &LDChunk,
    SmallPtrSetImpl<LoadInst*> &ProcessedLoads) {
  // If the chunk consists of only one or less loads we can skip as we cannot
  // do anything
  if (LDChunk.size() <= 1) {
    DEBUG(dbgs() << "Skipping chunk with zero or one element!\n");
    return false;
  }

  // The load from the lowest address
  LoadInst* const LowestLD = LDChunk.back();

  // First of all we have to find the load that is executed first. Due to the
  // way we created the load chains we know that there must be such a load.
  LoadInst *DomLD = nullptr;
  uint64_t GapBytesToLoLD = 0;
  for (LoadInst* LD : LDChunk) {
    // The load has already been processed (maybe in an other chunk). So we do
    // not consider it furthermore.
    if (ProcessedLoads.count(LD) > 0) { continue; }

    bool dominatesAll = true;
    bool FromLowerAddress = false;
    uint64_t curGapBytes = 0;

    for (SmallVectorImpl<LoadInst*>::iterator CHIT = LDChunk.begin(),
        CHEND = LDChunk.end(); CHIT != CHEND; ++CHIT) {
      LoadInst *ChkLD = *CHIT;

      if (LD == ChkLD) {
        // Every load that we will check now loads from an address that is
        // higher than the address of the load we are checking.
        FromLowerAddress = true;
        continue;
      }
      if (FromLowerAddress == true) {
        // The load reads from an address that is lower than the load we are
        // checking so we add the alloc size of the loaded object
        curGapBytes = curGapBytes + DL->getTypeAllocSize(ChkLD->getType());
      }
      if (DT->dominates(LD, ChkLD) == false) {
        dominatesAll = false;
        break;
      }
    }
    if (dominatesAll == true) {
      DomLD = LD;
      GapBytesToLoLD = curGapBytes;
      break;
    }
  }
  if (DomLD == nullptr) { return false; }
  if (ProcessedLoads.count(DomLD) > 0) { return false; }
  DEBUG(dbgs() << "Dominating load in current chunk is: " << *DomLD << " @"
               << DomLD << "\n  Offset to lowest address is " << GapBytesToLoLD
               << " bytes.\n");

  bool Changed = false;
  // Now we can create the new load instruction that loads all elements of
  // the chunk at once
  uint64_t ChunkElemSzBits = 0;
  uint64_t ChunkElemSzBytes = 0;
  { // Compute the size (in bits and bytes) of the whole chunk
    const SCEV *prevLDAddress = nullptr;
    for (SmallVectorImpl<LoadInst*>::reverse_iterator CHRIT = LDChunk.rbegin(),
        CHREND = LDChunk.rend(); CHRIT != CHREND; ++CHRIT) {
      LoadInst *LD = *CHRIT;
      const SCEV *curLDAddress = SE->getSCEV(LD->getPointerOperand());
      if (prevLDAddress != nullptr) {
        const SCEV *LDAddrDiff = SE->getMinusSCEV(curLDAddress, prevLDAddress);
        if (LDAddrDiff->isZero() == false) {
          // The two loads are loading from different memory locations
          ChunkElemSzBits += DL->getTypeAllocSizeInBits(LD->getType());
          ChunkElemSzBytes += DL->getTypeAllocSize(LD->getType());
        } else {
          // The two loads are loading from the same location so they will use the
          // same bitmask later on
        }
        prevLDAddress = curLDAddress;
      } else {
        // This is the first load we are processing...
        ChunkElemSzBits += DL->getTypeAllocSizeInBits(LD->getType());
        ChunkElemSzBytes += DL->getTypeAllocSize(LD->getType());
        prevLDAddress = curLDAddress;
      }
    }
  }

  LLVMContext &GlobalC = getGlobalContext();
  // The type that the new load instruction loads
  IntegerType *NewLDIntTy = IntegerType::get(GlobalC, ChunkElemSzBits);
  // The pointer type that points to the loaded value
  PointerType *NewLDPtrTy = PointerType::get(NewLDIntTy,
      LowestLD->getPointerAddressSpace());

  Instruction *NewLoadPtrOp = nullptr;
  if (DomLD == LowestLD) {
    // The dominating load is the load from the lowest address within the
    // chunk. So everything is fine...
    // Loads in proper order so no instruction movements needed
    NewLoadPtrOp = CastInst::CreatePointerCast(DomLD->getPointerOperand(),
        NewLDPtrTy, "", DomLD);
  } else {
    // The dominating load is not loading from the lowest address from within
    // the chunk. So we have to inject additional address computation.
    // Normally we would move the computation of the lowest address before
    // the dominating load. Therefore we have to care that the instructions
    // (that should be moved) do not have any side-effects or that they are moved
    // beyond instructions having side-effects themselfes. In both cases it
    // would be forbidden to move the instructions.
    // To avoid that we cheat a bit: as the loads are loading from coalescing
    // locations (with no gaps between them) we can compute the address of
    // the lowest address by subtracting the sizes of the elements between the
    // dominating load and the load from the lowest address. That computation is
    // free of side-effects and we do not have to move any instructions :)
    // The drawback is that we have ptrtoint and inttoptr instructions in the
    // code that seem to confuse the alias analysis a bit.

    if (isa<Constant>(LowestLD->getPointerOperand()) == true) {
      // The loaded address is constant that should be easy
      DEBUG(dbgs() << "  Found CONSTANT lowest address\n");
      NewLoadPtrOp = CastInst::CreatePointerCast(LowestLD->getPointerOperand(),
          NewLDPtrTy, "", DomLD);
      Changed = true;
    } else if (isa<Instruction>(LowestLD->getPointerOperand()) == true) {
      if (DT->dominates(dyn_cast<Instruction>(LowestLD->getPointerOperand()), DomLD) == true) {
        // No matter how, but everytime we reach the dominating load (that
        // dominates all other loads from the currently processed chunk) we
        // have already computed the address of the lowest load. So we
        // can just use that address after casting...
        DEBUG(dbgs() << "  Found DOMINATING lowest address\n");
        NewLoadPtrOp = CastInst::CreatePointerCast(LowestLD->getPointerOperand(),
            NewLDPtrTy, "", DomLD);
        Changed = true;
      } else {
        const unsigned movedInsts = Loopus::moveBefore(
            dyn_cast<Instruction>(LowestLD->getPointerOperand()), DomLD);
        if (movedInsts > 0) {
          // Try to move the pointer operand of the lowest load  before the
          // dominating load so that we can use it.
          NewLoadPtrOp = CastInst::CreatePointerCast(LowestLD->getPointerOperand(),
              NewLDPtrTy, "", DomLD);
          DEBUG(dbgs() << "  MOVED lowest address GEP with operands ("
                       << movedInsts << " instructions moved)\n");
          Changed = true;
        }
      }
    }
    // Nothing else worked so do it the hard way
    if (NewLoadPtrOp == nullptr) {
      DEBUG(dbgs() << "  Lowest address GEP NEITHER const NOR dominating NOR "
                   << "moved. Computing new address via sub!\n");
      // Our backup strategy:
      // First of all we need to cast the pointer into an int
      IntegerType *LDPtrIntTy = DL->getIntPtrType(GlobalC, LowestLD->getPointerAddressSpace());
      CastInst *CastedDomLDPtrInt = CastInst::CreatePointerCast(
          DomLD->getPointerOperand(), LDPtrIntTy, "", DomLD);

      // Now subtract the size of the elements at the lower addresses than the
      // dominating load
      ConstantInt *GapSizeCI = ConstantInt::get(LDPtrIntTy, GapBytesToLoLD, false);
      // I think we do not want NSW/NUW flag for the address computation
      BinaryOperator *NewLDIntOp = BinaryOperator::Create(Instruction::BinaryOps::Sub,
          CastedDomLDPtrInt, GapSizeCI, "");
      NewLDIntOp->insertAfter(CastedDomLDPtrInt);

      // Now cast it back into a pointer
      NewLoadPtrOp = new IntToPtrInst(NewLDIntOp, NewLDPtrTy, "");
      NewLoadPtrOp->insertAfter(NewLDIntOp);

      Changed = true;
    }
  }
  if (NewLoadPtrOp == nullptr) { return Changed; }
  LoadInst *NewLD = new LoadInst(NewLoadPtrOp, "merged.chunk.load");
  NewLD->insertAfter(NewLoadPtrOp);
  ++StatsNumCreatedLoads;
  // Copy and merge AliasAnalysis information for TBAA
  AAMDNodes NewLDTBAA;
  LowestLD->getAAMetadata(NewLDTBAA, false);

  // Now we have to process each load
  uint64_t OffsetToLowestLD = 0;
  const SCEV *prevLDAddress = nullptr;
  // We are iterating in reverse order to start with the load from the lowest
  // address
  for (SmallVectorImpl<LoadInst*>::reverse_iterator CHRIT = LDChunk.rbegin(),
      CHREND = LDChunk.rend(); CHRIT != CHREND; ++CHRIT) {
    LoadInst* const CurLD = *CHRIT;
    const uint64_t CurLDElemSzBits = DL->getTypeAllocSizeInBits(CurLD->getType());
    DEBUG(dbgs() << "  Processing load: " << *CurLD << " @" << CurLD << "\n");

    // Merge TBAA info with already given one
    CurLD->getAAMetadata(NewLDTBAA, true);

    // First we have to create the bitmask for the current load
    const APInt CurLDAPMask = APInt::getAllOnesValue(CurLDElemSzBits).
        zextOrSelf(NewLDIntTy->getBitWidth());
    DEBUG(dbgs() << "    apmk: bits="; printAPInt(CurLDAPMask, dbgs(), 16, 16); dbgs() << "\n");

    // Now determine how many bits we have to shift the mask
    APInt CurLDMemMask;
    uint64_t ShiftLen = 0;
    if (DL->isLittleEndian() == true) {
      ShiftLen = OffsetToLowestLD;
      CurLDMemMask = CurLDAPMask << ShiftLen;
    } else if (DL->isBigEndian() == true) {
      ShiftLen = ((ChunkElemSzBits - OffsetToLowestLD) - CurLDElemSzBits)
          + (CurLDElemSzBits - DL->getTypeSizeInBits(CurLD->getType()));
      CurLDMemMask = CurLDAPMask << ShiftLen;
    }

    DEBUG(
      const unsigned BWF = (ChunkElemSzBits + 3) / 4;
      dbgs() << "    mask: slen=" << ShiftLen << "\n"
             << "          mask="; printAPInt(CurLDMemMask, dbgs(), BWF, 16); dbgs() << "\n"
    );

    // Compute offset between the lowest address of the chunk and the load
    // address in the next loop iteration
    const SCEV *curLDAddress = SE->getSCEV(CurLD->getPointerOperand());
    if (prevLDAddress != nullptr) {
      const SCEV *LDAddrDiff = SE->getMinusSCEV(curLDAddress, prevLDAddress);
      if (LDAddrDiff->isZero() == false) {
        OffsetToLowestLD = OffsetToLowestLD + CurLDElemSzBits;
      }
      prevLDAddress = curLDAddress;
    } else {
      OffsetToLowestLD = OffsetToLowestLD + CurLDElemSzBits;
      prevLDAddress = curLDAddress;
    }

    if (ProcessedLoads.count(CurLD) > 0) {
      DEBUG(dbgs() << "   Skipping. Load already processed.\n");
      continue;
    }

    // Now inject the bitmask operations (bitwise-and)
    ConstantInt *MaskLDCI = dyn_cast<ConstantInt>(ConstantInt::get(NewLDIntTy, CurLDMemMask));
    BinaryOperator *MaskLDInst = BinaryOperator::Create(Instruction::BinaryOps::And,
        NewLD, MaskLDCI, "");
    MaskLDInst->setDebugLoc(CurLD->getDebugLoc());
    MaskLDInst->insertBefore(CurLD);

    // Now shift the results
    Instruction *ShiftLDValInst = nullptr;
    if (ShiftLen > 0) {
      ConstantInt *ShiftLenLDCI = ConstantInt::get(NewLDIntTy, ShiftLen, false);
      ShiftLDValInst = BinaryOperator::Create(Instruction::BinaryOps::LShr,
          MaskLDInst, ShiftLenLDCI, "");
      ShiftLDValInst->setDebugLoc(CurLD->getDebugLoc());
      ShiftLDValInst->insertAfter(MaskLDInst);
    } else {
      ShiftLDValInst = MaskLDInst;
    }

    // Now the cast operations: the needed cast depends on the target type: for
    // integral types it is enough to truncate the shifted value to the target
    // size. For FP types an other IntToFP cast has to be performed.
    CastInst *CastShiftValInst = nullptr;
    if (CurLD->getType()->isIntegerTy() == true) {
      CastShiftValInst = CastInst::CreateIntegerCast(ShiftLDValInst,
          CurLD->getType(), false, "");
      CastShiftValInst->insertAfter(ShiftLDValInst);
    } else if (CurLD->getType()->isFloatingPointTy() == true) {
      IntegerType *TruncedIntTy = IntegerType::get(GlobalC, CurLDElemSzBits);
      CastInst *TruncIntCast = CastInst::CreateIntegerCast(ShiftLDValInst,
          TruncedIntTy, false, "");
      TruncIntCast->setDebugLoc(CurLD->getDebugLoc());
      TruncIntCast->insertAfter(ShiftLDValInst);

      CastShiftValInst = new BitCastInst(TruncIntCast, CurLD->getType());
      CastShiftValInst->insertAfter(TruncIntCast);
    }
    if (CastShiftValInst == nullptr) { continue; }
    // CastInst *CastShiftValInst = CastInst::CreateTruncOrBitCast(ShiftLDValInst,
    //     CurLD->getType());
    if (CurLD->hasName() == true) {
      CastShiftValInst->setName(CurLD->getName());
    }
    CastShiftValInst->setDebugLoc(CurLD->getDebugLoc());

    // Now replace and erase the old load
    CurLD->replaceAllUsesWith(CastShiftValInst);
    ProcessedLoads.insert(CurLD);
    // Loopus::eraseFromParentRecursively(CurLD);

    ++StatsNumAggregatedLoads;
  } // End of load instruction

  DEBUG(dbgs() << "  Load chunk aggregated.\n");
  // Set TBAA info for new load
  NewLD->setAAMetadata(NewLDTBAA);

  return Changed;
}

//===- Implementation of LLVM pass ----------------------------------------===//
INITIALIZE_PASS_BEGIN(AggregateLoads, "loopus-aggmema", "Aggregate memory accesses",  false, false)
INITIALIZE_AG_DEPENDENCY(AliasAnalysis)
INITIALIZE_PASS_DEPENDENCY(DataLayoutPass)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_DEPENDENCY(LoopInfo)
INITIALIZE_PASS_DEPENDENCY(ScalarEvolution)
INITIALIZE_PASS_END(AggregateLoads, "loopus-aggmema", "Aggregate memory accesses",  false, false)

char AggregateLoads::ID = 0;

namespace llvm {
  Pass* createAggregateLoadsPass() {
    return new AggregateLoads();
  }
}

AggregateLoads::AggregateLoads(void)
 : FunctionPass(ID), LimitToBB(true), AA(nullptr), DL(nullptr), DT(nullptr),
   LLI(nullptr), MFN(nullptr), SE(nullptr) {
  initializeAggregateLoadsPass(*PassRegistry::getPassRegistry());
}

void AggregateLoads::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<AliasAnalysis>();
  AU.addRequired<DataLayoutPass>();
  AU.addPreserved<DataLayoutPass>();
  AU.addRequired<DominatorTreeWrapperPass>();
  AU.addPreserved<DominatorTreeWrapperPass>();
  AU.addRequired<LoopInfo>();
  AU.addPreserved<LoopInfo>();
  AU.addRequired<ScalarEvolution>();
  AU.setPreservesCFG();
}

bool AggregateLoads::runOnFunction(Function &F) {
  AA = &getAnalysis<AliasAnalysis>();
  DL = &getAnalysis<DataLayoutPass>().getDataLayout();
  DT = &getAnalysis<DominatorTreeWrapperPass>().getDomTree();
  LLI = &getAnalysis<LoopInfo>();
  MFN = &Loopus::MangledFunctionNames::getInstance();
  SE = &getAnalysis<ScalarEvolution>();
  if ((AA == nullptr) || (DL == nullptr) || (DT == nullptr) || (LLI == nullptr)
   || (MFN == nullptr) || (SE == nullptr)) {
    return false;
  }

  DEBUG(dbgs() << "Inspecting function: " << F.getName() << ":\n");

  // First of all get the load chains
  LoadChainMapTy LoadChainsMap;
  populateLoadChain(F, LoadChainsMap);

  // Now extract all load chains
  SmallVector<LoadChainTy, 8> LoadChains;
  while (LoadChainsMap.empty() == false) {
    LoadInst *nextLoad = LoadChainsMap.begin()->first;
    LoadChainTy curLoadChain;
    unsigned LDChainSz = computeLoadChain(LoadChainsMap, nextLoad, curLoadChain);
    if (LDChainSz < 1) {
      // At least the load instruction itself should be in the chain!!! If not
      // something strange happened and we bail out early and avoid any changes.
      llvm_unreachable("There must at least be one chain!");
    }
    // Now erase all loads of the current chain from the map
    for (LoadInst* LDChainLoad : curLoadChain) {
      LoadChainsMap.erase(LDChainLoad);
    }
    // Remember the current chain
    LoadChains.push_back(curLoadChain);
  }

  // Now divide each load chain into several chunks where each chunk can be
  // loaded by one single load instruction
  typedef SmallVector<LoadChainChunkTy, 2> ChunkListTy;
  // This list contains an entry for each load chain. And each entry consists of
  // a list of chunks. And each chunk is a list of load instructions (so a list
  // in a list in a list).
  SmallVector<ChunkListTy, 8> LoadChainChunks;
  for (SmallVector<LoadChainTy, 8>::iterator LCIT = LoadChains.begin(),
      LCEND = LoadChains.end(); LCIT != LCEND; ++LCIT) {
    LoadChainTy curLoadChain = *LCIT;
    ChunkListTy curLoadChainChunks;
    unsigned numChunks = splitIntoLoadChainChunks(curLoadChain, curLoadChainChunks);
    if (numChunks < 1) {
      // There should be at least one chunk when all loads fit into one single
      // load. So we again bail out as we did not touch anything yet and the
      // analysis results seem to be inconsistent.
      llvm_unreachable("There must at least be one chunk!");
    }
    LoadChainChunks.push_back(curLoadChainChunks);
  }

  DEBUG(
    dbgs() << "Found the following " << LoadChainChunks.size() << " chain(s):\n";
    for (ChunkListTy &LDChainChunks : LoadChainChunks) {
      dbgs() << "  - Chain consists of the following chunks:\n";
      for (LoadChainChunkTy &LDChunk : LDChainChunks) {
        dbgs() << "    - Chunk:\n";
        for (LoadInst *LD : LDChunk) {
          dbgs() << "      - " << *LD << "\n";
        }
      }
    }
  );

  // Now we have the different chunks and each chunk consists of loads sorted
  // in descending order. So the load from the highest address (within one
  // single chunk) is at the first index. We now have to process each chunk.

  bool Changed = false;
  SmallPtrSet<LoadInst*, 32> ProcessedLoads;
  for (ChunkListTy &LDChainChunkList : LoadChainChunks) {
    for (LoadChainChunkTy LDChainChunk : LDChainChunkList) {
      Changed |= handleChunk(LDChainChunk, ProcessedLoads);
    }
  }
  // Now erase all processed loads
  for (LoadInst *LD : ProcessedLoads) {
    Loopus::eraseFromParentRecursively(LD);
  }

  return Changed;
}
