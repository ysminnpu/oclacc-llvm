//===- SplitBarrierBlocks.cpp ---------------------------------------------===//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "hdl-splitbarrierblocks"

#include "SplitBarrierBlocks.h"

#include "MangledFunctionNames.h"

#include "llvm/ADT/Statistic.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include <algorithm>
#include <string>

using namespace llvm;

// If this option is set (to true) the barrier function call will seperated into
// its own block (besides the terminator). If it is unset (false) the call will
// be the last instruction before the terminator.
cl::opt<bool> SplitIntoOwnBlock("seperatebarriers", cl::desc("Move barrier function calls into their own blocks."), cl::Optional, cl::init(false));

STATISTIC(StatsNumBarriersSeen, "Number of found barrier function calls.");
STATISTIC(StatsNumBlocksCreated, "Number of created blocks.");

//===- Implementation of helper functions ---------------------------------===//
/// \brief Determines if the given instruction is a barrier function call.
bool SplitBarrierBlocks::isBarrierFunctionCall(const Instruction *I) const {
  if (I == 0) { return false; }
  const CallInst *CI = dyn_cast<CallInst>(I);
  if (CI == 0) { return false; }
  const std::string funcname = CI->getCalledFunction()->getName();
  const Loopus::MangledFunctionNames &FNames = Loopus::MangledFunctionNames::getInstance();
  if (FNames.isSynchronizationFunction(funcname) == true) {
    return true;
  } else {
    return false;
  }
}

/// \brief Scans the CFG and collects all blocks that contain a function call
/// \brief to a barrier function.
/// \returns The number of found blocks using barriers
///
/// Scans all blocks in \c F for barrier function calls. Each block containing
/// such a call is inserted into \c SplitBlocks no matter how many times a
/// barrier function is called in the block. The number of detected blocks is
/// returned.
unsigned SplitBarrierBlocks::collectSplitBlocks(Function &F,
    SmallVectorImpl<BasicBlock*> &SplitBlocks) {
  unsigned NumSplitBlocks = 0;
  for (Function::iterator BBIT = F.begin(), BBEND = F.end(); BBIT != BBEND;
      ++BBIT) {
    // Now scan the instructions
    for (BasicBlock::iterator INSIT = BBIT->begin(), INSEND = BBIT->end();
        INSIT != INSEND; ++INSIT) {
      const Instruction *I = &*INSIT;
      if (I == 0) { continue; }
      if (I->getOpcode() != Instruction::Call) { continue; }
      if (isBarrierFunctionCall(I) == true) {
        SplitBlocks.push_back(&*BBIT);
        ++NumSplitBlocks;
        break;
      }
    }
  }
  return NumSplitBlocks;
}

/// \brief Splits the block into two or three blocks at the first barrier usage.
/// \param BB The block to split.
///
/// Scans the block for the first barrier function and splits the block at that
/// position. Depending on the options the barrier function call is moved into
/// a new third block or remains in \c BB. A pointer to the new block (following
/// the block containing the barrier function call) is returned in case of
/// success. If \c BB does not contain any barrier function calls or in case of
/// an error \c null is returned.
/// Note that the new block still might contain barrier function calls and
/// should be scaned again.
BasicBlock* SplitBarrierBlocks::splitOneBlock(BasicBlock* BB) {
	if (BB == 0) { return 0; }

	// SplitPoint should point to the first instruction being in the new block
	const BasicBlock::iterator BBBEG = BB->begin();
	const BasicBlock::iterator BBEND = BB->end();
	BasicBlock::iterator SplitPoint = BBEND;
	bool doDoubleSplit = false;
	bool returnNewBlock = true;

	for (BasicBlock::iterator INSIT = BBBEG, INSEND = std::prev(BBEND);
			INSIT != INSEND; ++INSIT) {
		Instruction *I = &*INSIT;
		if (I == 0) { continue; }
		if (I->getOpcode() != Instruction::Call) { continue; }
		if  (isBarrierFunctionCall(I) == true) {
			++StatsNumBarriersSeen;
			if (INSIT == BBBEG) {
				// The call is the first instruction so SplitPoint should move to the
				// successor of I. Like this the call will be in its own block.
				SplitPoint = std::next(INSIT);
				doDoubleSplit = false;
				returnNewBlock = true;
			} else if (INSIT == std::prev(BBEND, 2)) {
				// The call is the last instruction before the terminator.
				if (SplitIntoOwnBlock == true) {
					// The call should be moved into its own block so SplitPoint must
					// point to the call itself.
					SplitPoint = INSIT;
					doDoubleSplit = false;
					returnNewBlock = false;
				} else {
					// The call should remain in the old block so we keep there and do not
					// create a new block.
					SplitPoint = BBEND;
					doDoubleSplit = false;
					returnNewBlock = false;
				}
			} else if (INSIT == std::prev(BBEND)) {
				// SHOULD NOT HAPPEN but just to be sure
				SplitPoint = BBEND;
				doDoubleSplit = false;
				returnNewBlock = false;
			} else {
				// The between the first instruction and the terminator. So now we have
				// to determine if the call should go into its own block or not.
				if (SplitIntoOwnBlock == true) {
					// The call should go into its own block. So SplitPoint must point to
					// the call itself as it will be moved into a new block.
					SplitPoint = INSIT;
					doDoubleSplit = true;
					returnNewBlock = true;
				} else {
					// The call should be the last instruction befor the terminator in the
					// old block. So SplitPoint must point to the first instruction after
					// the call.
					SplitPoint = std::next(INSIT);
					doDoubleSplit = false;
					returnNewBlock = true;
				}
			}
			break;
		}
	}

	// Check if we should split at the terminator
	if ((SplitPoint == BBEND) || (SplitPoint == std::prev(BBEND))) { return 0; }

	// SplitPoint now points to the instruction that should be the first in the
	// new block. So perform the first split
	DEBUG(dbgs() << "Splitting block " << BB->getName() << " the 1. time\n");
	Instruction *SplitInstruction = &*SplitPoint;
	BasicBlock *newBlock = SplitBlock(BB, SplitInstruction, this);
	if (newBlock == 0) { return newBlock; }
	++StatsNumBlocksCreated;

	if (doDoubleSplit == true) {
		// We should perform a second split after the first instruction in the
		// previouls created new block
		DEBUG(dbgs() << "Splitting block " << newBlock->getName() << " (due to "
				<< "block " << BB->getName() << ") the 2. time\n");
		SplitInstruction = &*(std::next(newBlock->begin()));
		newBlock = SplitBlock(newBlock, SplitInstruction, this);
		if (newBlock != 0) { ++StatsNumBlocksCreated; } 
	}

	if (returnNewBlock == true) {
		return newBlock;
	} else {
		return 0;
	}
}

/// \brief Splits all blocks in function \c F using barrier functions.
bool SplitBarrierBlocks::splitAllBlocks(Function &F) {
  // We split those blocks in two steps: first we collect all blocks using
  // barriers and after we split them. We do that as everything else would
  // invalidate the iterator over the basic blocks.
  SmallVector<BasicBlock*, 10> splitBlocks;
  if (collectSplitBlocks(F, splitBlocks) == 0) {
    // There are no blocks to split so bail out
    return false;
  }

  bool changed = false;

  // Now split those blocks
  for (BasicBlock* SBB : splitBlocks) {
    BasicBlock *nextSBlock = SBB;
    do {
      // Split nextSBlock. If a new block was created check that block again...
      nextSBlock = splitOneBlock(nextSBlock);
      if (nextSBlock != 0) { changed = true; }
    } while(nextSBlock != 0);
  }

  return changed;
}

//===- Implementation of LLVM pass ----------------------------------------===//
INITIALIZE_PASS_BEGIN(SplitBarrierBlocks, "loopus-splitbarb", "Split barrier blocks", false, false)
INITIALIZE_PASS_END(SplitBarrierBlocks, "loopus-splitbarb", "Split barrier blocks", false, false)

char SplitBarrierBlocks::ID = 0;

namespace llvm {
  Pass* createSplitBarrierBlocksPass() {
    return new SplitBarrierBlocks();
  }
}
SplitBarrierBlocks::SplitBarrierBlocks(void)
 : FunctionPass(ID) {
  initializeSplitBarrierBlocksPass(*PassRegistry::getPassRegistry());
}

void SplitBarrierBlocks::getAnalysisUsage(AnalysisUsage &AU) const {
}

bool SplitBarrierBlocks::runOnFunction(Function &F) {
  return splitAllBlocks(F);
}
