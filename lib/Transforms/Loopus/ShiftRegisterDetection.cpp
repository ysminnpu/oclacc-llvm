//===- ShiftRegisterDetection.cpp -----------------------------------------===//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "loopus-srdetect"

#include "ShiftRegisterDetection.h"

#include "LoopusUtils.h"

#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/GlobalAlias.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include <iterator>
#include <set>

using namespace llvm;

STATISTIC(StatsNumDetectedSRLoops, "Number of detected shift register loops");
STATISTIC(StatsNumIntrinsicCalls, "Number of instrinsic calls inserted");

// If this option is set the pass will not add the unroll.disable metadata for
// a shiftregister loop.
cl::opt<bool> NoUnrollDisableOnShiftReg("nodisunrollsr", cl::desc("Do not add metadata on shiftregisters to disable loop unrolling."), cl::Optional, cl::init(false));
// If this options is set the pass will not remove any invalid metadata
// concerning shiftregisters. Note that the unroll.disable metadata will not be
// although this options is not given.
cl::opt<bool> NoMetadataRemoval("nomdremoval", cl::desc("Do not remove metadata concerning shiftregister although they might be inconsistent."), cl::Optional, cl::init(false));
// If this option is set detected full shift reegister loops are not replaced
// by an intrinsic call.
cl::opt<bool> NoReplaceIntrinsics("nosregintr", cl::desc("Do not replace full shift register loops by an intrinsic call."), cl::Optional, cl::init(false));

//===- Implementation of detection functions ------------------------------===//
/// \brief Returns \c true if the given instruction adds one to the given operand.
bool ShiftRegisterDetection::isAddOne(const Instruction *I, const Value *Op) const {
  if ((I == 0) || (Op == 0)) { return false; }
  if (isa <BinaryOperator>(I) == false) { return false; }

  const BinaryOperator *AI = dyn_cast<BinaryOperator>(I);
  if (AI->getOpcode() == Instruction::BinaryOps::Add) {
    const ConstantInt *CIOne = 0;
    if (AI->getOperand(0) == Op) {
      // Second operand must be a constant 1
      const Value *SOp = AI->getOperand(1);
      if (isa<ConstantInt>(SOp) == true) {
        CIOne = dyn_cast<ConstantInt>(SOp);
      }
    } else if (AI->getOperand(1) == Op) {
      // First operand must be a constant 1
      const Value *FOp = AI->getOperand(0);
      if (isa<ConstantInt>(FOp) == true) {
        CIOne = dyn_cast<ConstantInt>(FOp);
      }
    }
    if (CIOne != 0) {
      if (CIOne->isOne() == true) {
        return true;
      }
    }
  } else if (AI->getOpcode() == Instruction::BinaryOps::Sub) {
    const ConstantInt *CIMinusOne = 0;
    if (AI->getOperand(0) == Op) {
      // Second operand must be a constant -1
      const Value *SOp = AI->getOperand(1);
      if (isa<ConstantInt>(SOp) == true) {
        CIMinusOne = dyn_cast<ConstantInt>(SOp);
      }
    }
    if (CIMinusOne != 0) {
      if (CIMinusOne->isMinusOne() == true) {
        return true;
      }
    }
  }
  return false;
}

/// \brief Returns \c true if the given instruction subtracts one from the given operand.
bool ShiftRegisterDetection::isSubOne(const Instruction *I, const Value *Op) const {
  if ((I == 0) || (Op == 0)) { return false; }
  if (isa <BinaryOperator>(I) == false) { return false; }

  const BinaryOperator *AI = dyn_cast<BinaryOperator>(I);
  if (AI->getOpcode() == Instruction::BinaryOps::Add) {
    const ConstantInt *CIMinusOne = 0;
    if (AI->getOperand(0) == Op) {
      // Second operand must be a constant -1
      const Value *SOp = AI->getOperand(1);
      if (isa<ConstantInt>(SOp) == true) {
        CIMinusOne = dyn_cast<ConstantInt>(SOp);
      }
    } else if (AI->getOperand(1) == Op) {
      // First operand must be a constant -1
      const Value *FOp = AI->getOperand(0);
      if (isa<ConstantInt>(FOp) == true) {
        CIMinusOne = dyn_cast<ConstantInt>(FOp);
      }
    }
    if (CIMinusOne != 0) {
      if (CIMinusOne->isMinusOne() == true) {
        return true;
      }
    }
  } else if (AI->getOpcode() == Instruction::BinaryOps::Sub) {
    const ConstantInt *CIOne = 0;
    if (AI->getOperand(0) == Op) {
      // Second operand must be a constant 1
      const Value *SOp = AI->getOperand(1);
      if (isa<ConstantInt>(SOp) == true) {
        CIOne = dyn_cast<ConstantInt>(SOp);
      }
    }
    if (CIOne != 0) {
      if (CIOne->isOne() == true) {
        return true;
      }
    }
  }
  return false;
}

/// If the given instruction is an add instruction then the other operand of
/// the sum will be returned if it is a constant and strictly positive. If the
/// given instruction is a sub instruction then always (!) the negated second
/// operand is returned if it is negative and if the first operand is the given
/// operand.
const Constant* ShiftRegisterDetection::getPositiveOpOfAdd(
    const BinaryOperator *Add, const Value *Op) const {
  if ((Add == 0) || (Op == 0)) { return 0; }

  if (Add->getOpcode() == Instruction::BinaryOps::Add) {
    const Value *OtherOp = 0;
    if (Add->getOperand(0) == Op) {
      OtherOp = Add->getOperand(1);
    } else if (Add->getOperand(1) == Op) {
      OtherOp = Add->getOperand(0);
    }
    if (OtherOp == 0) { return 0; }
    const ConstantInt *CIOtherOp = dyn_cast<ConstantInt>(OtherOp);
    if (CIOtherOp == 0) { return 0; }
    if (CIOtherOp->getValue().isStrictlyPositive() == false) { return 0; }
    return CIOtherOp;
  } else if (Add->getOpcode() == Instruction::BinaryOps::Sub) {
    if (Add->getOperand(0) != Op) { return 0; }
    const ConstantInt *CIOtherOp = dyn_cast<ConstantInt>(Add->getOperand(1));
    if (CIOtherOp == 0) { return 0; }
    if (CIOtherOp->getValue().isNegative() == false) { return 0; }
    const Constant *CIPositiveOOp = ConstantInt::get(
        CIOtherOp->getType(), -(CIOtherOp->getValue()));
    return CIPositiveOOp;
  }
  return 0;
}

/// If the given instruction is an sub instruction then the second operand of
/// the subtraction will be returned if it is a constant and strictly positive.
/// If the given instruction is an add instruction then the negated other
/// operand is returned if it is negative.
const Constant* ShiftRegisterDetection::getPositiveOpOfSub(
    const BinaryOperator *Sub, const Value *Op) const {
  if ((Sub == 0) || (Op == 0)) { return 0; }

  if (Sub->getOpcode() == Instruction::BinaryOps::Add) {
    const Value *OtherOp = 0;
    if (Sub->getOperand(0) == Op) {
      OtherOp = Sub->getOperand(1);
    } else if (Sub->getOperand(1) == Op) {
      OtherOp = Sub->getOperand(0);
    }
    if (OtherOp == 0) { return 0; }
    const ConstantInt *CIOtherOp = dyn_cast<ConstantInt>(OtherOp);
    if (CIOtherOp == 0) { return 0; }
    if (CIOtherOp->getValue().isNegative() == false) { return 0; }
    const Constant *CIPositiveOOp = ConstantInt::get(
        CIOtherOp->getType(), -(CIOtherOp->getValue()));
    return CIPositiveOOp;
  } else if (Sub->getOpcode() == Instruction::BinaryOps::Sub) {
    if (Sub->getOperand(0) != Op) { return 0; }
    const ConstantInt *CIOtherOp = dyn_cast<ConstantInt>(Sub->getOperand(1));
    if (CIOtherOp == 0) { return 0; }
    if (CIOtherOp->getValue().isStrictlyPositive() == false) { return 0; }
    return CIOtherOp;
  }
  return 0;
}

bool ShiftRegisterDetection::isAllowedType(const Type* Ty) const {
  if (Ty == 0) { return false; }
  if ((Ty->isFloatingPointTy() == true)
   || (Ty->isIntegerTy() == true)
   || (Ty->isPointerTy() == true)
   || (Ty->isStructTy() == true)) {
    return true;
  }
  return false;
}

bool ShiftRegisterDetection::isSafeBaseArray(const llvm::Value *V) const {
  if (V == 0) { return false; }

  SmallPtrSet<const Value*, 6> VisitedVals;
  while (VisitedVals.count(V) == 0) {
    DEBUG(dbgs() << "Testing " << *V << " for validity...");
    if (isa<Instruction>(V) == true) {
      const Instruction *I = dyn_cast<Instruction>(V);
      DEBUG(dbgs() << " opc=" << I->getOpcode());
    } else if (isa<Constant>(V) == true) {
        DEBUG(dbgs() << " const");
    }

    // As stated in the OpenCL specification: "Every OpenCL C function-scope
    // local variable is mapped to an LLVM module-level variable in address
    // space 3". Especially they are not allocated using an alloca
    // instruction as those currently allocate memory in the LLVM (!) generic
    // address space 0. Currently alloca cannot create variables in any other
    // (and especially not in the local) address space. That means that every
    // variable allocated by alloca is created in the OpenCL private address
    // space.
    VisitedVals.insert(V);
    if (isa<AllocaInst>(V) == true) {
      const AllocaInst *AI = dyn_cast<AllocaInst>(V);
      DEBUG(dbgs() << " ty=" << *AI->getType());
      DEBUG(dbgs() << " at=" << *AI->getAllocatedType());
      DEBUG(dbgs() << " sz=" << *AI->getArraySize());
      const Type *AllocTy = 0;
      if (AI->isArrayAllocation() == false) {
        // As this is no array allocation only one array is allocated and
        // therefore cannot be multidimensional.
        if (AI->getAllocatedType()->isArrayTy() == true) {
          AllocTy = AI->getAllocatedType()->getArrayElementType();
        } else if (AI->getAllocatedType()->isPointerTy() == true) {
          AllocTy = AI->getAllocatedType()->getPointerElementType();
        }
      } else {
        AllocTy = AI->getAllocatedType();
      }
      // Test that the type is valid
      if ((AI->getType()->getPointerAddressSpace() == Loopus::OpenCLAddressSpaces::ADDRSPACE_LOCAL)
       || (AI->getType()->getPointerAddressSpace() == Loopus::OpenCLAddressSpaces::ADDRSPACE_PRIVATE)) {
        if ((AllocTy != 0) && (isAllowedType(AllocTy) == true)) {
          DEBUG(dbgs() << " [passed]\n");
          return true;
        }
      }
    } else if (isa<GetElementPtrInst>(V) == true) {
      const GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(V);
      if (GEP->hasAllZeroIndices() == true) {
        DEBUG(dbgs() << " [gpcont]\n");
        V = GEP->getPointerOperand();
      }
    } else if (isa<AddrSpaceCastInst>(V) == true) {
      DEBUG(dbgs() << " Address space casts are not allowed for shift "
          << "register base arrays!");
    } else if (isa<BitCastInst>(V) == true) {
      DEBUG(dbgs() << " [bccont]\n");
      const BitCastInst *BCI = dyn_cast<BitCastInst>(V);
      V = BCI->getOperand(0);
    } else if (isa<Constant>(V) == true) {
      if (isa<ConstantExpr>(V) == true) {
        const ConstantExpr *CE = dyn_cast<ConstantExpr>(V);
        if (CE->getOpcode() == Instruction::CastOps::AddrSpaceCast) {
          DEBUG(dbgs() << " Address space casts are not allowed for shift "
              << "register base arrays!");
        } else if (CE->getOpcode() == Instruction::CastOps::BitCast) {
          // This is a constant bitcast
          DEBUG(dbgs() << " [bccoct]\n");
          V = CE->getOperand(0);
        } else if (CE->getOpcode() == Instruction::MemoryOps::GetElementPtr) {
          bool allZero = true;
          for (unsigned i = 1, e = CE->getNumOperands(); i < e; ++i) {
            if (ConstantInt *CIIdx = dyn_cast<ConstantInt>(CE->getOperand(i))) {
              if (CIIdx->isZero() == false) {
                allZero = false;
                break;
              }
            } else {
              allZero = false;
              break;
            }
          }
          if (allZero == true) {
            DEBUG(dbgs() << " [gpcoct]\n");
            V = CE->getOperand(0);
          }
        }
      } else if (isa<GlobalAlias>(V) == true) {
        // Global alias found so de-alias it and keep checking...
        DEBUG(dbgs() << " [gacont]\n");
        const GlobalAlias *CGA = dyn_cast<GlobalAlias>(V);
        V = CGA->getAliasee();
      } else if (isa<GlobalVariable>(V) == true) {
        // Global variable found. Note that in LLVM global variables are always
        // pointers. So we first have to strip off the pointer
        const GlobalVariable *CGV = dyn_cast<GlobalVariable>(V);
        // Global variables are ALWAYS pointers in llvm!
        if (CGV->getType()->isPointerTy() == false) {
          errs() << "Hmm, strange global variable (not of ptr ty) found!\n";
          return false;
        }
        const Type *CGVTy = CGV->getType()->getPointerElementType();
        DEBUG(dbgs() << " ty=" << *CGV->getType()->getPointerElementType());
        if (CGVTy->isArrayTy() == true) {
          CGVTy = CGVTy->getArrayElementType();
        } else if (CGVTy->isPointerTy() == true) {
          CGVTy = CGVTy->getPointerElementType();
        }
        if ((CGV->getType()->getPointerAddressSpace() == Loopus::OpenCLAddressSpaces::ADDRSPACE_LOCAL)
         || (CGV->getType()->getPointerAddressSpace() == Loopus::OpenCLAddressSpaces::ADDRSPACE_PRIVATE)) {
          if ((CGVTy != 0) && (isAllowedType(CGVTy) == true)) {
            DEBUG(dbgs() << " [passed]\n");
            return true;
          }
        }
      }
    } // End of isa<Constant>
  } // End of while-loop
  DEBUG(dbgs() << " [FAILED]\n");
  return false;
}

bool ShiftRegisterDetection::checkLoopWrapper(Loop *L, ShiftRegister *SROut) {
  struct ShiftRegister BestFoundSR;
  bool BestFoundResult = false;
  for (Loop::block_iterator BIT = L->block_begin(), BEND = L->block_end();
      BIT != BEND; ++BIT) {
    const BasicBlock* const BB = *BIT;
    for (BasicBlock::const_iterator INSIT = BB->begin(), INSEND = BB->end();
        INSIT != INSEND; ++INSIT) {
      if (isa<StoreInst>(&*INSIT) == true) {
        const StoreInst *SI = dyn_cast<StoreInst>(&*INSIT);
        if (isa<LoadInst>(SI->getValueOperand()) == true) {
          const LoadInst *LI = dyn_cast<LoadInst>(SI->getValueOperand());

          struct ShiftRegister tmpSR;
          bool tmpResult = checkLoop(L, SI, LI, &tmpSR);
          if (tmpSR.Result == LoopResult::FullSR) {
            BestFoundSR = tmpSR;
            BestFoundResult = tmpResult;
          } else if (tmpSR.Result == LoopResult::PartialSR) {
            if (BestFoundSR.Result == LoopResult::NoSR) {
              BestFoundSR = tmpSR;
              BestFoundResult = tmpResult;
            }
          }
        }
      }
    }
    if (BestFoundSR.Result == LoopResult::FullSR) { break; }
  }
  if (SROut != 0) {
    *SROut = BestFoundSR;
  }
  return BestFoundResult;
}

// bool ShiftRegisterDetection::checkLoop(Loop *L, ShiftRegister *SROut) {
bool ShiftRegisterDetection::checkLoop(Loop *L, const StoreInst *StInst,
    const LoadInst *LdInst, ShiftRegister *SROut) {
  // This function tries to detect shift register loops. A loop that can be
  // transformed into a shift register must have the following form:
  // for (int i = C0; i < C1; ++i) {
  //   sr[i-C0] = sr[i];
  // }
  // Such a loop shifts each element C0 positions to the left. C1 is the length
  // of the shift register.
  // Any other instructions are currently not allowed to be contained in the
  // loop body as that would required loop distribution to seperate the shift
  // register instructions from the remaining loop body.
  // The above loop would be translated into some IR code like this:
  // for.cond
  //   %i = phi [C0, label %entry] [%i.inc, label %for.inc]
  //   %cmp = icmp ult %i, C1
  //   br i1 %cmp, label %for.body, label %for.end
  // for.body
  //   %loadptr = getelementptr %sr, %i
  //   %0 = load %loadptr
  //   %i.sub = sub %i, 1
  //   %storeptr = getelementptr %sr, %i.sub
  //   store %0, %storeptr
  //   br label %for.inc
  // for.inc
  //   %i.inc = add %i, 1
  //   br label %for.cond
  // for.end
  // So the important points are: there must be a PHI node introducing the
  // value and the shift length. Then there must be the compare- and conditional
  // branch instruction. Then there must be the load- and gep-instruction. And
  // index-computation and the gep- and store instruction.
  //
  // Note the load and store instructions need pointer that are pointing into
  // the local address space.
  // First check all basic block: the predecessors of each bb (except the header)
  // must be in the loop. As any conditions are forbidden all predecessors must
  // be single.
  DEBUG(dbgs() << "=====>>INSPECTING loop at " << L->getHeader()->getName() << ":\n");
  struct ShiftRegister SR(L);
  if (SROut != 0) {
    *SROut = SR;
  }

  const BasicBlock* const LHBB = L->getHeader();
  // Check the basic structure of the loop
  for (Loop::block_iterator BIT = L->block_begin(), BEND = L->block_end();
      BIT != BEND; ++BIT) {
    const BasicBlock* const BB = *BIT;
    if (BB == 0) { continue; }

    if (BB != LHBB) {
      const BasicBlock* const PredBB = BB->getSinglePredecessor();
      // Check that there is a single predecessor
      if (PredBB == 0) { return false; }
      // That predecessor must be part of the loop
      if (L->contains(PredBB) == false) { return false; }
    }

    if (L->isLoopExiting(BB) == false) {
      // If block is no exiting block then the block must have a single successor
      const BasicBlock* const SuccBB = Loopus::getSingleSuccessor(BB);
      if (SuccBB == 0) { return false; }
      if (L->contains(SuccBB) == false) { return false; }
    } else {
      // There must not be any other exits than the loop condition
      if (SR.LoopBranch != 0) {
        DEBUG(dbgs() << "Already found loop branch/condition!\n");
        return false;
      }

      const BranchInst* const BI = dyn_cast<BranchInst>(BB->getTerminator());
      if (BI == 0) { return false; }
      if (BI->isConditional() == true) {
        SR.LoopBranch = BI;
        SR.LoopCond = BI->getCondition();
      } else {
        DEBUG(dbgs() << "Hmm, strange: exiting block without conditional "
            << "branch as terminator!\n");
        return false;
      }
    }
  }
  if ((SR.LoopBranch == 0) || (SR.LoopCond == 0)) {
    DEBUG(dbgs() << "Did not find loop branch and condition!\n");
    return false;
  }
  if (SROut != 0) {
    *SROut = SR;
  }

  // Now search the main instructions that make up such a shift register loop
  // First look for the load and store instruction
  if ((StInst == 0) || (LdInst == 0)) {
    return false;
  }
  if ((L->contains(StInst->getParent()) == false)
   || (L->contains(LdInst->getParent()) == false)) {
    DEBUG(dbgs() << "Load or store instruction not contained in loop!\n");
    return false;
  }
  SR.RegStore = StInst;
  SR.RegLoad = LdInst;
  DEBUG(dbgs() << "Using store: " << *SR.RegStore << "\n");
  DEBUG(dbgs() << "Using load: " << *SR.RegLoad << "\n");
  if ((SR.RegLoad == 0) || (SR.RegStore == 0)) {
    DEBUG(dbgs() << "Did not find load and store instruction!\n");
    return false;
  }
  if (SROut != 0) {
    *SROut = SR;
  }

  // Now let's try to find the loop counter: the loop counter is the result of
  // a PHI node: the one input is the initial value and the second operand is
  // the value incremented by (always!) one.
  // But as the loop header might contain other PHI nodes we try to detect the
  // loop counter based on the load/store instruction. One of them must directly
  // use the loop counter as index.
  const GetElementPtrInst* const LdGep = dyn_cast<GetElementPtrInst>(SR.RegLoad->getPointerOperand());
  if (LdGep == 0) { return false; }
  SR.BaseArray = LdGep->getPointerOperand();
  if (SR.BaseArray->getType()->isPointerTy() == false) {
    DEBUG(dbgs() << "Something went wrong: GEP with no pointer op found!\n");
    return false;
  }
  if (SROut != 0) {
    *SROut = SR;
  }
  DEBUG(dbgs() << "Found basearray (ld): " << *SR.BaseArray << "\n");
  const GetElementPtrInst* const StGep = dyn_cast<GetElementPtrInst>(SR.RegStore->getPointerOperand());
  if (StGep == 0) { return false; }
  if (StGep->getPointerOperand()->getType()->isPointerTy() == false) {
    DEBUG(dbgs() << "Something went wrong: GEP with no pointer op found!\n");
    return false;
  }

  // Check for valid types
  if (isAllowedType(SR.RegLoad->getType()) == false) {
    DEBUG(dbgs() << "Type of shifted values is not allowed for shifting!\n");
    return false;
  }
  // Now make sure that the store always stores the value loaded by the load.
  // This also makes sure, that the load is always executed before the store
  if (SR.RegStore->getValueOperand() != SR.RegLoad) {
    DEBUG(dbgs() << "Store does not use loaded value!\n");
    return false;
  }
  if (StGep->isSameOperationAs(LdGep) == false) {
    DEBUG(dbgs() << "LdGep and StGep are not same!\n");
    return false;
  }
  // Check that the load and store address the same array
  if (StGep->getPointerOperand() != SR.BaseArray) {
    DEBUG(dbgs() << "Load and Store basearrays do not match!\n");
    return false;
  }
  // Make sure that shift register is valid
  if (isSafeBaseArray(SR.BaseArray) == false) {
    DEBUG(dbgs() << "Base array is not safe to be used as shift register!\n");
    return false;
  }
  if ((SR.BaseArray->getType()->getPointerAddressSpace() != Loopus::OpenCLAddressSpaces::ADDRSPACE_LOCAL)
   && (SR.BaseArray->getType()->getPointerAddressSpace() != Loopus::OpenCLAddressSpaces::ADDRSPACE_PRIVATE)) {
    DEBUG(dbgs() << "Base array is neither located in local ("
        << Loopus::OpenCLAddressSpaces::ADDRSPACE_LOCAL << ") "
        << "nor in private ("
        << Loopus::OpenCLAddressSpaces::ADDRSPACE_PRIVATE << ") "
        << "address space!\n");
    return false;
  }

  // Check the dimensions and type of the array:
  // - Only 1D arrays are allowed.
  // - In most cases we will have a pointer to an array: as the array must be
  //   local (according to SPIR spec) it is created as global variable and
  //   therefore is of pointer type.
  enum TyInfo {Unknown = 0, PlainPtr, PlainArray, PtrToArray};
  TyInfo BaTyInfo = TyInfo::Unknown;

  const Type *BaTy = SR.BaseArray->getType();
  if (BaTy->isPointerTy() == true) {
    BaTy = BaTy->getPointerElementType();
    BaTyInfo = TyInfo::PlainPtr;

    if (BaTy->isArrayTy() == true) {
      BaTy = BaTy->getArrayElementType();
      BaTyInfo = TyInfo::PtrToArray;
    }
  } else if (BaTy->isArrayTy() == true) {
    BaTy = BaTy->getArrayElementType();
    BaTyInfo = TyInfo::PlainArray;
  }
  if (isAllowedType(BaTy) == false) {
    DEBUG(dbgs() << "Type of base array values is not allowed for shifting!\n");
    return false;
  }
  // Now try to determine the array size
  int tmparrsz = -1;
  if (BaTyInfo == TyInfo::PlainArray) {
    tmparrsz = SR.BaseArray->getType()->getArrayNumElements();
  } else if (BaTyInfo == TyInfo::PtrToArray) {
    tmparrsz = SR.BaseArray->getType()->getPointerElementType()->getArrayNumElements();
  }
  const int ArraySize = tmparrsz;

  int LCIndex = 0;
  if ((BaTyInfo == TyInfo::PlainPtr)
   || (BaTyInfo == TyInfo::PlainArray)) {
    // Only one index is needed to dereference the base array. So the GEPs must
    // have two operands as the source pointer is also counted.
    if ((LdGep->getNumOperands() != 2)
     || (StGep->getNumOperands() != 2)) {
      DEBUG(dbgs() << "Invalid derefernce for load or store!\n");
      return false;
    }
    LCIndex = 0;
  } else if (BaTyInfo == TyInfo::PtrToArray) {
    // Two indices are needed to dereference the base array. So the GEPs must
    // have three operands as the source pointer is also counted.
    if ((LdGep->getNumOperands() != 3)
     || (StGep->getNumOperands() != 3)) {
      DEBUG(dbgs() << "Invalid derefernce for load or store!\n");
      return false;
    }
    LCIndex = 1;
  } else {
    DEBUG(dbgs() << "Unknown array dereference type found!\n");
    return false;
  }

  if (BaTyInfo == TyInfo::PtrToArray) {
    // At this point we have to check that the first index is zero. That index
    // dereferences the pointer that points to the array. Especially for global
    // variables there is only one single element (the array) at the pointed
    // position and anything else than zero would be invalid. Nevertheless if
    // the pointer points to an array of arrays the access would be illegal as
    // shift register.
    const ConstantInt* const LdIdx0 = dyn_cast<ConstantInt>(LdGep->getOperand(1));
    if (LdIdx0 == 0) {
      DEBUG(dbgs() << "First index for load access is no const int!\n");
      return false;
    }
    if (LdIdx0->isZero() == false) {
      DEBUG(dbgs() << "First index for load access is not zero!\n");
      return false;
    }
    const ConstantInt* const StIdx0 = dyn_cast<ConstantInt>(StGep->getOperand(1));
    if (StIdx0 == 0) {
      DEBUG(dbgs() << "First index for store access is no const int!\n");
      return false;
    }
    if (StIdx0->isZero() == false) {
      DEBUG(dbgs() << "First index for store access is not zero!\n");
      return false;
    }
  }

  const Value* const LdIndex = LdGep->getOperand(LCIndex+1);
  const Value* const StIndex = StGep->getOperand(LCIndex+1);
  if (isa<PHINode>(LdIndex) == true) {
    SR.LoopCounter = dyn_cast<PHINode>(LdIndex);
    DEBUG(dbgs() << "Used loop index (ld): " << *LdIndex << "\n");
  } else if (isa<PHINode>(StIndex) == true) {
    SR.LoopCounter = dyn_cast<PHINode>(StIndex);
    DEBUG(dbgs() << "Used loop index (st): " << *StIndex << "\n");
  } else {
    DEBUG(dbgs() << "No simple loop index detected!\n");
    return false;
  }
  // Check the existence of the loop counter. Note that either the load or the
  // store instruction directly uses the loop counter as index.
  if (SR.LoopCounter == 0) {
    DEBUG(dbgs() << "Could not find loop counter!\n");
    return false;
  }
  if (SROut != 0) {
    *SROut = SR;
  }

  // The initial value of the loop counter
  const Value *InitLoopCounterV = 0;
  // The instruction that increments the loop counter
  const Value *IncLoopCounterV = 0;
  for (unsigned i = 0, e = SR.LoopCounter->getNumIncomingValues(); i < e; ++i) {
    const BasicBlock *IncomingBB = SR.LoopCounter->getIncomingBlock(i);
    const Value *IncomingV = SR.LoopCounter->getIncomingValueForBlock(IncomingBB);
    if (L->contains(IncomingBB) == true) {
      // The block is part of the loop so the incoming value is the increment
      // instruction for the loop counter
      if (IncLoopCounterV == 0) {
        IncLoopCounterV = IncomingV;
      } else {
        if (IncLoopCounterV != IncomingV) {
          DEBUG(dbgs() << "Different incoming values from inside loop for counter!\n");
          return false;
        }
      }
    } else {
      // The block is not part of the loop so the incoming value is the initial
      // value for the loop counter
      if (InitLoopCounterV == 0) {
        InitLoopCounterV = IncomingV;
      } else {
        if (InitLoopCounterV != IncomingV) {
          DEBUG(dbgs() << "Different incoming values from outisde loop for counter!\n");
          return false;
        }
      }
    }
  }
  if ((IncLoopCounterV == 0) || (InitLoopCounterV == 0)) {
    DEBUG(dbgs() << "No loop counter initialization or update!\n");
    return false;
  }
  // The initial loop counter value must be a constant and the increment must
  // be an instruction.
  const ConstantInt* const InitLoopCounter = dyn_cast<ConstantInt>(InitLoopCounterV);
  const Instruction* const IncLoopCounter = dyn_cast<Instruction>(IncLoopCounterV);
  if (InitLoopCounter == 0) {
    DEBUG(dbgs() << "Initial loop counter is no constant int!\n");
    return false;
  }
  if (IncLoopCounter == 0) {
    DEBUG(dbgs() << "Loop counter increment is no instruction!\n");
    return false;
  }
  DEBUG(dbgs() << "Loop counter: init=" << *InitLoopCounter
      << " ; inc=" << *IncLoopCounter << "\n");

  // Determine the direction of the shift
  // The direction is correct as the load or the store instruction are using
  // the loop counter as index. So something like
  // for (i = 0; i < len; i++) {
  //   i[len-i-1] = i[len-i-2];
  // }
  // is NOT possible: either the load or the store must directly use the loop
  // counter.
  enum ShiftDir {Undef = 0, LeftInc, RightDec};
  // ShiftDir ShDir = ShiftDir::Undef;
  if (isAddOne(IncLoopCounter, SR.LoopCounter) == true) {
    // ShDir = ShiftDir::LeftInc;
    SR.ShDir = ShiftDirection::ArrShlInc;
  } else if (isSubOne(IncLoopCounter, SR.LoopCounter) == true) {
    // ShDir = ShiftDir::RightDec;
    SR.ShDir = ShiftDirection::ArrShrDec;
  } else {
    DEBUG(dbgs() << "Invalid shift direction detected!\n");
    SR.ShDir = ShiftDirection::Undef;
    return false;
  }
  if (SROut != 0) {
    *SROut = SR;
  }

  // Now determine the shift length. At the same make sure that the loop really
  // performs a shift in the assumed direction.
  // if (ShDir == ShiftDir::LeftInc) {
  if (SR.ShDir == ShiftDirection::ArrShlInc) {
    if ((SR.LoopCounter == LdIndex)
     && (isa<BinaryOperator>(StIndex) == true)) {
      // The load instruction directly uses the loop counter as index so the
      // index of the store is computed. The store must be the subtraction of a
      // positive number of the addition of a negative one.
      const BinaryOperator *BIdx = dyn_cast<BinaryOperator>(StIndex);
      const Constant *SLen = getPositiveOpOfSub(BIdx, SR.LoopCounter);
      if (isa<ConstantInt>(SLen) == true) {
        SR.ShiftLength = dyn_cast<ConstantInt>(SLen);
      }
    } else if ((SR.LoopCounter == StIndex)
     && (isa<BinaryOperator>(LdIndex) == true)) {
      // Now the store instruction directly uses the loop counter so same tests
      // as above but with load and store swaped
      const BinaryOperator *BIdx = dyn_cast<BinaryOperator>(LdIndex);
      const Constant *SLen = getPositiveOpOfAdd(BIdx, SR.LoopCounter);
      if (isa<ConstantInt>(SLen) == true) {
        SR.ShiftLength = dyn_cast<ConstantInt>(SLen);
      }
    } else {
      DEBUG(dbgs() << "Unable to detect shift length!\n");
      return false;
    }
  // } else if (ShDir == ShiftDir::RightDec) {
  } else if (SR.ShDir == ShiftDirection::ArrShrDec) {
    if ((SR.LoopCounter == LdIndex)
     && (isa<BinaryOperator>(StIndex) == true)) {
      // The load instruction directly uses the loop counter as index so the
      // index of the store is computed. The store must be an addition of a
      // positive number or a subtraction of a negative one.
      const BinaryOperator *BIdx = dyn_cast<BinaryOperator>(StIndex);
      const Constant *SLen = getPositiveOpOfAdd(BIdx, SR.LoopCounter);
      if (isa<ConstantInt>(SLen) == true) {
        SR.ShiftLength = dyn_cast<ConstantInt>(SLen);
      }
    } else if ((SR.LoopCounter == StIndex)
     && (isa<BinaryOperator>(LdIndex) == true)) {
      const BinaryOperator *BIdx = dyn_cast<BinaryOperator>(LdIndex);
      const Constant *SLen = getPositiveOpOfSub(BIdx, SR.LoopCounter);
      if (isa<ConstantInt>(SLen) == true) {
        SR.ShiftLength = dyn_cast<ConstantInt>(SLen);
      }
    } else {
      DEBUG(dbgs() << "Unable to detect shift length!\n");
      return false;
    }
  } else {
    DEBUG(dbgs() << "Unknown shift direction detected!\n");
    return false;
  }
  if (SR.ShiftLength == 0) {
    DEBUG(dbgs() << "Invalid shift length or direction detected!\n");
    return false;
  }
  if (SROut != 0) {
    *SROut = SR;
  }
  // if (ShDir == ShiftDir::LeftInc) {
  if (SR.ShDir == ShiftDirection::ArrShlInc) {
    DEBUG(dbgs() << "Shifting left (inc) by " << *SR.ShiftLength << " (const@"
        << SR.ShiftLength << ") positions!\n");
  // } else if (ShDir == ShiftDir::RightDec) {
  } else if (SR.ShDir == ShiftDirection::ArrShrDec) {
    DEBUG(dbgs() << "Shifting right (dec) by " << *SR.ShiftLength << " (const@"
        << SR.ShiftLength << ") positions!\n");
  }

  // Make sure that we have a proper comparison
  if (isa<ICmpInst>(SR.LoopCond) == false) {
    DEBUG(dbgs() << "Loop condition is no icmp instruction!\n");
    return false;
  }
  const ICmpInst* const CondCmp = dyn_cast<ICmpInst>(SR.LoopCond);
  // Determine the shift length (as usable int)
  int ShiftLenI = 0;
  if (CondCmp->isSigned() == true) {
    ShiftLenI = SR.ShiftLength->getSExtValue();
  } else {
    ShiftLenI = SR.ShiftLength->getZExtValue();
  }
  // Determine the opindex of the loop counter and the comparison value
  // We are allowing the incremented loop counter here to be able to evaluate
  // loops rotated by the loop-rotate pass too.
  int LCIdx = -1;
  int OOIdx = -1;
  if ((CondCmp->getOperand(0) == SR.LoopCounter)
   || (CondCmp->getOperand(0) == IncLoopCounter)) {
    LCIdx = 0; OOIdx = 1;
  } else if ((CondCmp->getOperand(1) == SR.LoopCounter)
   || (CondCmp->getOperand(1) == IncLoopCounter)) {
    LCIdx = 1; OOIdx = 0;
  } else {
    DEBUG(dbgs() << "Comparison must contain plain loop counter!\n");
    return false;
  }
  // Now check the loop conditions
  int InitExpectedVal = -1;
  int CondExpectedVal = -1;
  // if (ShDir == ShiftDir::LeftInc) {
  if (SR.ShDir == ShiftDirection::ArrShlInc) {
    // When shifting left there are two possibilities for the loop condition
    // 1. init=shiftlen ; upperlimit(excl.)=arraysize
    // 2. init=0 ; upperlimit(excl.)=arraysize-shiftlen
    if (SR.LoopCounter == LdIndex) {
      // Case 1
      InitExpectedVal = ShiftLenI;
      if (CondCmp->getPredicate() == CmpInst::Predicate::ICMP_NE) {
        CondExpectedVal = ArraySize;
      } else if ((CondCmp->getPredicate() == CmpInst::Predicate::ICMP_ULT)
       || (CondCmp->getPredicate() == CmpInst::Predicate::ICMP_SLT)) {
        if ((LCIdx != 0) || (OOIdx != 1)) {
          DEBUG(dbgs() << "Malformed loop comparison detected "
              << "(Try swapping the operands)!\n");
          return false;
        }
        CondExpectedVal = ArraySize;
      } else if ((CondCmp->getPredicate() == CmpInst::Predicate::ICMP_ULE)
       || (CondCmp->getPredicate() == CmpInst::Predicate::ICMP_SLE)) {
        if ((LCIdx != 0) || (OOIdx != 1)) {
          DEBUG(dbgs() << "Malformed loop comparison detected "
              << "(Try swapping the operands)!\n");
          return false;
        }
        CondExpectedVal = ArraySize - 1;
      } else if ((CondCmp->getPredicate() == CmpInst::Predicate::ICMP_UGT)
       || (CondCmp->getPredicate() == CmpInst::Predicate::ICMP_SGT)) {
        if ((LCIdx != 1) || (OOIdx != 0)) {
          DEBUG(dbgs() << "Malformed loop comparison detected "
              << "(Try swapping the operands)!\n");
          return false;
        }
        CondExpectedVal = ArraySize;
      } else if ((CondCmp->getPredicate() == CmpInst::Predicate::ICMP_UGE)
       || (CondCmp->getPredicate() == CmpInst::Predicate::ICMP_SGE)) {
        if ((LCIdx != 1) || (OOIdx != 0)) {
          DEBUG(dbgs() << "Malformed loop comparison detected "
              << "(Try swapping the operands)!\n");
          return false;
        }
        CondExpectedVal = ArraySize - 1;
      } else {
        DEBUG(dbgs() << "Unexpected comparison predicate found! "
            << "Use one of !=, <, <=, >, >=\n");
        return false;
      }
    } else if (SR.LoopCounter == StIndex) {
      // Case 2
      InitExpectedVal = 0;
      if (CondCmp->getPredicate() == CmpInst::Predicate::ICMP_NE) {
        CondExpectedVal = ArraySize - ShiftLenI;
      } else if ((CondCmp->getPredicate() == CmpInst::Predicate::ICMP_ULT)
       || (CondCmp->getPredicate() == CmpInst::Predicate::ICMP_SLT)) {
        if ((LCIdx != 0) || (OOIdx != 1)) {
          DEBUG(dbgs() << "Malformed loop comparison detected "
              << "(Try swapping the operands)!\n");
          return false;
        }
        CondExpectedVal = ArraySize - ShiftLenI;
      } else if ((CondCmp->getPredicate() == CmpInst::Predicate::ICMP_ULE)
       || (CondCmp->getPredicate() == CmpInst::Predicate::ICMP_SLE)) {
        if ((LCIdx != 0) || (OOIdx != 1)) {
          DEBUG(dbgs() << "Malformed loop comparison detected "
              << "(Try swapping the operands)!\n");
          return false;
        }
        CondExpectedVal = ArraySize - ShiftLenI - 1;
      } else if ((CondCmp->getPredicate() == CmpInst::Predicate::ICMP_UGT)
       || (CondCmp->getPredicate() == CmpInst::Predicate::ICMP_SGT)) {
        if ((LCIdx != 1) || (OOIdx != 0)) {
          DEBUG(dbgs() << "Malformed loop comparison detected "
              << "(Try swapping the operands)!\n");
          return false;
        }
        CondExpectedVal = ArraySize - ShiftLenI;
      } else if ((CondCmp->getPredicate() == CmpInst::Predicate::ICMP_UGE)
       || (CondCmp->getPredicate() == CmpInst::Predicate::ICMP_SGE)) {
        if ((LCIdx != 1) || (OOIdx != 0)) {
          DEBUG(dbgs() << "Malformed loop comparison detected "
              << "(Try swapping the operands)!\n");
          return false;
        }
        CondExpectedVal = ArraySize - ShiftLenI - 1;
      } else {
        DEBUG(dbgs() << "Unexpected comparison predicate found! "
            << "Use one of !=, <, <=, >, >=\n");
        return false;
      }
    } else {
      DEBUG(dbgs() << "Hmm... Should not happen anymore!\n");
      return false;
    }
  // } else if (ShDir == ShiftDir::RightDec) {
  } else if (SR.ShDir == ShiftDirection::ArrShrDec) {
    // When shifting right there are two possibilities for the loop condition
    // 1. init=arraysize-1-shiftlen ; upperlimit(incl.)=0
    // 2. init=arraysize-1 ; upperlimit(incl.)=shiftlen
    if (SR.LoopCounter == LdIndex) {
      // Case 1
      InitExpectedVal = ArraySize - ShiftLenI - 1;
      if (CondCmp->getPredicate() == CmpInst::Predicate::ICMP_NE) {
        CondExpectedVal = -1;
      } else if ((CondCmp->getPredicate() == CmpInst::Predicate::ICMP_ULT)
       || (CondCmp->getPredicate() == CmpInst::Predicate::ICMP_SLT)) {
        if ((LCIdx != 1) || (OOIdx != 0)) {
          DEBUG(dbgs() << "Malformed loop comparison detected "
              << "(Try swapping the operands)!\n");
          return false;
        }
        CondExpectedVal = -1;
      } else if ((CondCmp->getPredicate() == CmpInst::Predicate::ICMP_ULE)
       || (CondCmp->getPredicate() == CmpInst::Predicate::ICMP_SLE)) {
        if ((LCIdx != 1) || (OOIdx != 0)) {
          DEBUG(dbgs() << "Malformed loop comparison detected "
              << "(Try swapping the operands)!\n");
          return false;
        }
        CondExpectedVal = 0;
      } else if ((CondCmp->getPredicate() == CmpInst::Predicate::ICMP_UGT)
       || (CondCmp->getPredicate() == CmpInst::Predicate::ICMP_SGT)) {
        if ((LCIdx != 0) || (OOIdx != 1)) {
          DEBUG(dbgs() << "Malformed loop comparison detected "
              << "(Try swapping the operands)!\n");
          return false;
        }
        CondExpectedVal = -1;
      } else if ((CondCmp->getPredicate() == CmpInst::Predicate::ICMP_UGE)
       || (CondCmp->getPredicate() == CmpInst::Predicate::ICMP_SGE)) {
        if ((LCIdx != 0) || (OOIdx != 1)) {
          DEBUG(dbgs() << "Malformed loop comparison detected "
              << "(Try swapping the operands)!\n");
          return false;
        }
        CondExpectedVal = 0;
      } else {
        DEBUG(dbgs() << "Unexpected comparison predicate found! "
            << "Use one of !=, <, <=, >, >=\n");
        return false;
      }
    } else if (SR.LoopCounter == StIndex) {
      // Case 2
      InitExpectedVal = ArraySize - 1;
      if (CondCmp->getPredicate() == CmpInst::Predicate::ICMP_NE) {
        CondExpectedVal = ShiftLenI - 1;
      } else if ((CondCmp->getPredicate() == CmpInst::Predicate::ICMP_ULT)
       || (CondCmp->getPredicate() == CmpInst::Predicate::ICMP_SLT)) {
        if ((LCIdx != 1) || (OOIdx != 0)) {
          DEBUG(dbgs() << "Malformed loop comparison detected "
              << "(Try swapping the operands)!\n");
          return false;
        }
        CondExpectedVal = ShiftLenI - 1;
      } else if ((CondCmp->getPredicate() == CmpInst::Predicate::ICMP_ULE)
       || (CondCmp->getPredicate() == CmpInst::Predicate::ICMP_SLE)) {
        if ((LCIdx != 1) || (OOIdx != 0)) {
          DEBUG(dbgs() << "Malformed loop comparison detected "
              << "(Try swapping the operands)!\n");
          return false;
        }
        CondExpectedVal = ShiftLenI;
      } else if ((CondCmp->getPredicate() == CmpInst::Predicate::ICMP_UGT)
       || (CondCmp->getPredicate() == CmpInst::Predicate::ICMP_SGT)) {
        if ((LCIdx != 0) || (OOIdx != 1)) {
          DEBUG(dbgs() << "Malformed loop comparison detected "
              << "(Try swapping the operands)!\n");
          return false;
        }
        CondExpectedVal = ShiftLenI - 1;
      } else if ((CondCmp->getPredicate() == CmpInst::Predicate::ICMP_UGE)
       || (CondCmp->getPredicate() == CmpInst::Predicate::ICMP_SGE)) {
        if ((LCIdx != 0) || (OOIdx != 1)) {
          DEBUG(dbgs() << "Malformed loop comparison detected "
              << "(Try swapping the operands)!\n");
          return false;
        }
        CondExpectedVal = ShiftLenI;
      } else {
        DEBUG(dbgs() << "Unexpected comparison predicate found! "
            << "Use one of !=, <, <=, >, >=\n");
        return false;
      }
    } else {
      DEBUG(dbgs() << "Hmm... Should not happen anymore!\n");
      return false;
    }
  }
  // Now the testing...
  if (ArraySize >= 0) {
    if (InitLoopCounter->getSExtValue() != InitExpectedVal) {
      DEBUG(dbgs() << "Invalid loop counter initialization "
          << "(initialize loop counter with " << InitExpectedVal << ")!\n");
      return false;
    }
    const ConstantInt* const CmpOp = dyn_cast<ConstantInt>(CondCmp->getOperand(OOIdx));
    if (CmpOp == 0) {
      DEBUG(dbgs() << "Comparison value must be a constant!\n");
      return false;
    }
    if (CmpOp->getSExtValue() != CondExpectedVal) {
    // if (CmpOp->equalsInt(CondExpectedVal) == false) {
      DEBUG(dbgs() << "Loop counter is not in array bounds (comparison "
          << "value should be " << CondExpectedVal << " and is s" << CmpOp->getSExtValue() << ")!\n");
      return false;
    }
  } else {
    // This happens when a pointer is used instead of an array
    DEBUG(dbgs() << "WARNING: Could not check loop counter for array bounds!\n");
  }
  DEBUG(dbgs() << "Array boundary check passed!\n");

  // Now the loop contains a shift register but it might be that there are more
  // instructions in the loop than the shift register
  SR.Result = LoopResult::PartialSR;
  if (SROut != 0) {
    *SROut = SR;
  }

  // Now we have to make sure that no other instructions than the shift are
  // performed in the loop. Therefore first collect all needed instructions
  // to do the shift.
  // Note that the terminator instructions of the blocks contained in the loop
  // are not added to the set.
  std::set<const Value*> SRLValues;
  // Needed: branch, compare, phi node for loop counter, loop counter increment
  SRLValues.insert(SR.LoopBranch);
  SRLValues.insert(SR.LoopCond);
  SRLValues.insert(SR.LoopCounter);
  SRLValues.insert(IncLoopCounter);
  // The load, store, corresponding geps and their indices (note that one of
  // indices is already inserted as it is either the loop counter or its
  // increment)
  SRLValues.insert(SR.RegLoad);
  SRLValues.insert(LdGep);
  SRLValues.insert(LdIndex);
  SRLValues.insert(SR.RegStore);
  SRLValues.insert(StGep);
  SRLValues.insert(StIndex);

  // Show me what you have
  DEBUG(dbgs() << "Valid insts in loop:\n";
    for (auto INSIT = SRLValues.cbegin(), INSEND = SRLValues.cend();
        INSIT != INSEND; ++INSIT) {
      dbgs() << "  - @" << *INSIT << ": " << **INSIT << "\n";
    }
  );

  // Now test all instructions in the loop
  for (Loop::block_iterator BIT = L->block_begin(), BEND = L->block_end();
      BIT != BEND; ++BIT) {
    const BasicBlock* const BB = *BIT;
    for (BasicBlock::const_iterator INSIT = BB->begin(), INSEND = BB->end();
        INSIT != INSEND; ++INSIT) {
      const Instruction* const I = &*INSIT;
      if (I == BB->getTerminator()) {
        // As the set does not contain the block terminators we have to skip
        // them manually
        continue;
      }
      if (SRLValues.count(I) == 0) {
        DEBUG(dbgs() << "Instruction not concerned with shift found in loop!\n");
        return false;
      }
    }
  }

  // As I don't know any more test the loop should contain only the shift
  // register
  SR.Result = LoopResult::FullSR;
  if (SROut != 0) {
    SROut->Result = LoopResult::FullSR;
  }
  return true;
}

MDNode* ShiftRegisterDetection::findLoopMetadata(Loop *L,
    const std::string &MDName) const {
  if (L == 0) { return 0; }

  MDNode *LID = L->getLoopID();
  if (LID == 0) { return 0; }
  if (LID->getNumOperands() <= 1) { return 0; }
  if (LID->getOperand(0) != LID) { return 0; }

  for (unsigned i = 1, e = LID->getNumOperands(); i < e; ++i) {
    MDNode *LMD = dyn_cast<MDNode>(LID->getOperand(i).get());
    if (LMD == 0) { continue; }

    if (LMD->getNumOperands() == 0) { continue; }
    const MDString *MDDesc = dyn_cast<MDString>(LMD->getOperand(0).get());
    if (MDDesc == 0) { continue; }

    if (MDDesc->getString().equals(MDName) == true) {
      return LMD;
    }
  }
  return 0;
}

bool ShiftRegisterDetection::hasLoopUnrollEnableMetadata(Loop *L) const {
  if (L == 0) { return false; }

  if ((findLoopMetadata(L, Loopus::LoopMDStrings::LLVM_UNROLL_ENABLE) != 0)
   || (findLoopMetadata(L, Loopus::LoopMDStrings::LLVM_UNROLL_FULL) != 0)
   || (findLoopMetadata(L, Loopus::LoopMDStrings::LLVM_UNROLL_COUNT) != 0)) {
    return true;
  } else {
    return false;
  }
}

void ShiftRegisterDetection::addLoopStringMetadata(Loop *L,
    const std::string &MDContents) const {
  if (L == 0) { return; }

  MDNode *LID = L->getLoopID();
  LLVMContext &GContext = getGlobalContext();
  if (LID == 0) {
    // Create new loop metadata to be used as loop ID
    SmallVector<Metadata*, 1> LoopIDAttrs;
    LoopIDAttrs.push_back(0); // This will later be replaced by the node itself
    MDNode *LoopID = MDNode::get(GContext, LoopIDAttrs);
    LoopID->replaceOperandWith(0, LoopID);
    L->setLoopID(LoopID);
  }
  LID = L->getLoopID();
  if (LID == 0) {
    DEBUG(errs() << "Failed to created LoopID for loop at "
        << L->getHeader()->getName() << "!\n");
    return;
  }

  // Now create the given metadata string. We are assuming that the given string
  // is NOT already part of the loop metadata node (that has to be ensured by
  // the user of this function).
  // As LLVM does not allow to directly add a new metadata node to an existing
  // one we have to create a new metadata id that replaces the current loop id.
  // Did I already mention that I do not like editing metadata nodes in LLVM!!!
  SmallVector<Metadata*, 4> NewLIDAttrs;
  for (unsigned i = 0, e = LID->getNumOperands(); i < e; ++i) {
    NewLIDAttrs.push_back(LID->getOperand(i).get());
  }
  SmallVector<Metadata*, 1> LMDSVector;
  LMDSVector.push_back(MDString::get(GContext, MDContents));
  MDNode *LMDSNode = MDNode::get(GContext, LMDSVector);
  NewLIDAttrs.push_back(LMDSNode);

  MDNode *NewLID = MDNode::get(GContext, NewLIDAttrs);
  NewLID->replaceOperandWith(0, NewLID);
  L->setLoopID(NewLID);
}

void ShiftRegisterDetection::removeLoopStringMetadata(llvm::Loop *L,
    const std::string &MDContents) const {
  if (L == 0) { return; }

  MDNode *LID = L->getLoopID();
  LLVMContext &GContext = getGlobalContext();
  if (LID == 0) { return; }

  SmallVector<Metadata*, 4> NewLIDAttrs;
  for (unsigned i = 0, e = LID->getNumOperands(); i < e; ++i) {
    MDNode *CurMDOp = dyn_cast<MDNode>(LID->getOperand(i).get());
    if (CurMDOp == 0) {
      // This is not even a MDNode so keep it
      NewLIDAttrs.push_back(LID->getOperand(i).get());
      continue;
    }
    const MDString *CurMDOpDesc = dyn_cast<MDString>(CurMDOp->getOperand(0).get());
    if (CurMDOpDesc == 0) {
      // The current operand has no description so we do not remove it
      NewLIDAttrs.push_back(CurMDOp);
      continue;
    }
    if (CurMDOpDesc->getString().equals(MDContents) == false) {
      // The current operand has a description but it does not match so kepp it
      NewLIDAttrs.push_back(CurMDOp);
      continue;
    }
  }

  MDNode *NewLID = MDNode::get(GContext, NewLIDAttrs);
  NewLID->replaceOperandWith(0, NewLID);
  L->setLoopID(NewLID);
}

void ShiftRegisterDetection::writeFullSRLoopMetadata(Loop *L) const {
  if (L == 0) { return; }
  // First remove all invalid shiftregister metadat
  if (NoMetadataRemoval == false) {
    removeLoopStringMetadata(L, Loopus::LoopMDStrings::LOOPUS_SHIFTREG_NONE);
    removeLoopStringMetadata(L, Loopus::LoopMDStrings::LOOPUS_SHIFTREG_PARTIAL);
  }
  // Now add the full shiftreg metadata
  if (findLoopMetadata(L, Loopus::LoopMDStrings::LOOPUS_SHIFTREG_FULL) == 0) {
    addLoopStringMetadata(L, Loopus::LoopMDStrings::LOOPUS_SHIFTREG_FULL);
  }
  // If wished add the unroll.disable metadata
  if (NoUnrollDisableOnShiftReg == false) {
    if (hasLoopUnrollEnableMetadata(L) == false) {
      if (findLoopMetadata(L, Loopus::LoopMDStrings::LLVM_UNROLL_DISABLE) == 0) {
        addLoopStringMetadata(L, Loopus::LoopMDStrings::LLVM_UNROLL_DISABLE);
      }
    }
  }
}

void ShiftRegisterDetection::writePartialSRLoopMetadata(Loop *L) const {
  if (L == 0) { return; }
  // First remove all invalid shiftregister metadat
  if (NoMetadataRemoval == false) {
    removeLoopStringMetadata(L, Loopus::LoopMDStrings::LOOPUS_SHIFTREG_NONE);
    removeLoopStringMetadata(L, Loopus::LoopMDStrings::LOOPUS_SHIFTREG_FULL);
  }
  // Now add the partial shiftreg metadata
  if (findLoopMetadata(L, Loopus::LoopMDStrings::LOOPUS_SHIFTREG_PARTIAL) == 0) {
    addLoopStringMetadata(L, Loopus::LoopMDStrings::LOOPUS_SHIFTREG_PARTIAL);
  }
}

bool ShiftRegisterDetection::replaceShiftLoop(ShiftRegister &SR) {
  if (SR.Result != LoopResult::FullSR) { return false; }
  if ((SR.ShDir != ShiftDirection::ArrShlInc)
   && (SR.ShDir != ShiftDirection::ArrShrDec)) {
    return false;
  }
  if (SR.Loop == nullptr) { return false; }
  if (SR.BaseArray == nullptr) { return false; }
  if (SR.LoopCounter == nullptr) { return false; }
  if (SR.LoopCond == nullptr) { return false; }
  if (SR.ShiftLength == nullptr) { return false; }

  // We need the loop bounds for the intrinsic call
  ConstantInt *InitLoopCounter = nullptr;
  for (unsigned i = 0, e = SR.LoopCounter->getNumIncomingValues(); i < e; ++i) {
    BasicBlock *IncomingBB = SR.LoopCounter->getIncomingBlock(i);
    Value *CurOp = SR.LoopCounter->getIncomingValueForBlock(IncomingBB);
    if (SR.Loop->contains(IncomingBB) == false) {
      InitLoopCounter = dyn_cast<ConstantInt>(CurOp);
    }
  }
  if (InitLoopCounter == nullptr) {
    DEBUG(dbgs() << "  Could not determine init loop counter value!\n");
    return false;
  }

  // Not determine the upper exclusive loop boundary
  ConstantInt *ExclLoopBound = nullptr;
  const ICmpInst *LoopCmp = dyn_cast<ICmpInst>(SR.LoopCond);
  if (LoopCmp == nullptr) { return false; }
  const ConstantInt *ConstCmpOp = dyn_cast<ConstantInt>(LoopCmp->getOperand(0));
  if (ConstCmpOp == nullptr) {
    ConstCmpOp = dyn_cast<ConstantInt>(LoopCmp->getOperand(1));
  }
  if (ConstCmpOp == nullptr) {
    DEBUG(dbgs() << "  Could not determine const int loop boundary!\n");
    return false;
  }

  // These predicates imply an inclusive loop boundary
  if ((LoopCmp->getPredicate() == CmpInst::Predicate::ICMP_ULE)
   || (LoopCmp->getPredicate() == CmpInst::Predicate::ICMP_SLE)
   || (LoopCmp->getPredicate() == CmpInst::Predicate::ICMP_UGE)
   || (LoopCmp->getPredicate() == CmpInst::Predicate::ICMP_SGE)) {
    APInt ExclLoopBoundAVal = ConstCmpOp->getValue() + 1;
    ExclLoopBound = ConstantInt::get(getGlobalContext(), ExclLoopBoundAVal);
  } else {
    ExclLoopBound = const_cast<ConstantInt*>(ConstCmpOp);
  }
  if (ExclLoopBound == nullptr) {
    DEBUG(dbgs() << "  Could not find const int excl loop bound!\n");
    return false;
  }

  // For replacing a shiftloop we add an intrinsic call to the exiting block
  // (previous checks should have found out that there is only one exiting
  // block). The header will branch to the exiting block directly (unconditionally)
  // and the exiting block will exit the loop directly. Other blocks are erased.
  Module *M = SR.Loop->getHeader()->getParent()->getParent();
  if (M == nullptr) { return false; }
  SmallVector<Type*, 4> ArgTypes;
  ArgTypes.push_back(SR.BaseArray->getType());
  ArgTypes.push_back(InitLoopCounter->getType());
  ArgTypes.push_back(ExclLoopBound->getType());
  ArgTypes.push_back(SR.ShiftLength->getType());
  Function *IntrF = nullptr;
  if (SR.ShDir == ShiftDirection::ArrShlInc) {
    IntrF = Intrinsic::getDeclaration(M, Intrinsic::ID::loopus_shl_reg, ArgTypes);
  } else if (SR.ShDir == ShiftDirection::ArrShrDec) {
    IntrF = Intrinsic::getDeclaration(M, Intrinsic::ID::loopus_shr_reg, ArgTypes);
  } else {
    DEBUG(dbgs() << "  Invalid shift direction found!\n");
    return false;
  }
  if (IntrF == nullptr) {
    DEBUG(dbgs() << "  Did not find intrinsic function decl!\n");
    return false;
  }

  // Now build the argument list
  SmallVector<Value*, 4> IntrArgs;
  IntrArgs.push_back(const_cast<Value*>(SR.BaseArray));
  IntrArgs.push_back(InitLoopCounter);
  IntrArgs.push_back(ExclLoopBound);
  IntrArgs.push_back(const_cast<ConstantInt*>(SR.ShiftLength));

  // Look for the exiting block
  BasicBlock* const LExitingBB = SR.Loop->getExitingBlock();
  if (LExitingBB == nullptr) {
    DEBUG(dbgs() << "  Loop has several exiting block!\n");
    return false;
  }
  BasicBlock* const LExitBB = SR.Loop->getExitBlock();
  if (LExitBB == nullptr) {
    DEBUG(dbgs() << "  Loop has several exit blocks!\n");
    return false;
  }
  BasicBlock* const LHeaderBB = SR.Loop->getHeader();

  // Now create the call instruction
  CallInst *IntrCI = CallInst::Create(IntrF, IntrArgs);

  // Now erase all instructions in all blocks
  LExitingBB->replaceSuccessorsPhiUsesWith(LHeaderBB);
  for (Loop::block_iterator LBBIT = SR.Loop->block_begin(),
      LBBEND = SR.Loop->block_end(); LBBIT != LBBEND; ) {
    BasicBlock *CurBB = *LBBIT; ++LBBIT;
    if (CurBB == nullptr) { continue; }

    // Erase all instructions in the current block except for the terminator
    BasicBlock::iterator BBINSIT = CurBB->begin();
    BasicBlock::iterator BBINSEND = CurBB->end();
    if (CurBB == LHeaderBB) { --BBINSEND; }
    for ( ; BBINSIT != BBINSEND ; ) {
      Instruction *CurI = &*BBINSIT;
      ++BBINSIT;
      UndefValue *UdefV = UndefValue::get(CurI->getType());
      CurI->replaceAllUsesWith(UdefV);
      CurI->eraseFromParent();
    }
  }
  // Now adapt the branch of the header
  BranchInst *NewHeadBI = BranchInst::Create(LExitBB);
  ReplaceInstWithInst(LHeaderBB->getTerminator(), NewHeadBI);
  // Now erase the blocks
  for (Loop::block_iterator LBBIT = SR.Loop->block_begin(),
      LBBEND = SR.Loop->block_end(); LBBIT != LBBEND; ) {
    BasicBlock *CurBB = *LBBIT; ++LBBIT;
    if (CurBB == nullptr) { continue; }
    if (CurBB != LHeaderBB) {
      CurBB->eraseFromParent();
    }
  }

  // Now insert the intrinsic call
  IntrCI->insertBefore(LHeaderBB->getTerminator());

  ++StatsNumIntrinsicCalls;

  return true;
}

bool ShiftRegisterDetection::handleLoop(Loop *L) {
  if (L == 0) { return false; }
  auto &Subloops = L->getSubLoops();

  bool changed = false;

  if (Subloops.size() > 0) {
    // If there any subloops this loop cannot be a shift register loop
    for (Loop *SL : Subloops) {
      changed |= handleLoop(SL);
    }
  } else {
    // If there are no subloops perform further checks
    struct ShiftRegister SR;
    bool isSR = checkLoopWrapper(L, &SR);
    if (isSR == true) {
      StatsNumDetectedSRLoops++;
      // Insert Intrinsic call here...
      if (NoReplaceIntrinsics == false) {
        changed = replaceShiftLoop(SR);
      }
      // End of intrinsic stuff
      if (changed == false) {
        // The loop was replaced by an intrinsic call so do not annotate anything
        writeFullSRLoopMetadata(L);
      }
      DEBUG(dbgs() << "Loop " << L->getHeader()->getName() << " is FULL shiftreg!\n");
    } else {
      if (SR.Result == LoopResult::PartialSR) {
        writePartialSRLoopMetadata(L);
        DEBUG(dbgs() << "Loop " << L->getHeader()->getName() << " is PARTIAL shiftreg!\n");
      } else {
        if (NoMetadataRemoval == false) {
          // Remove all shiftregister metadata
          removeLoopStringMetadata(L, Loopus::LoopMDStrings::LOOPUS_SHIFTREG_NONE);
          removeLoopStringMetadata(L, Loopus::LoopMDStrings::LOOPUS_SHIFTREG_PARTIAL);
          removeLoopStringMetadata(L, Loopus::LoopMDStrings::LOOPUS_SHIFTREG_FULL);
        }
        DEBUG(dbgs() << "Loop " << L->getHeader()->getName() << " is NO shiftreg!\n");
      }
    }
  }
  return changed;
}

//===- Implementation of LLVM pass ----------------------------------------===//
INITIALIZE_PASS_BEGIN(ShiftRegisterDetection, "loopus-srd", "Detect shift register loops",  false, false)
INITIALIZE_PASS_DEPENDENCY(LoopInfo)
INITIALIZE_PASS_END(ShiftRegisterDetection, "loopus-srd", "Detect shift register loops",  false, false)

char ShiftRegisterDetection::ID = 0;

namespace llvm {
  Pass* createShiftRegisterDetectionPass() {
    return new ShiftRegisterDetection();
  }
}

ShiftRegisterDetection::ShiftRegisterDetection()
 : FunctionPass(ID) {
  initializeShiftRegisterDetectionPass(*PassRegistry::getPassRegistry());
}

void ShiftRegisterDetection::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<LoopInfo>();
  AU.setPreservesAll();
}

bool ShiftRegisterDetection::runOnFunction(Function &F) {
  LoopInfo &LI = getAnalysis<LoopInfo>();
  bool changed = false;
  for (LoopInfo::iterator LIT = LI.begin(), LEND = LI.end(); LIT != LEND; ++LIT) {
    changed |= handleLoop(*LIT);
  }
  return changed;
}

