//===- HDLLoopUnroll.cpp --------------------------------------------------===//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "loopus-loop-unroll"

#include "HDLLoopUnroll.h"

#include "LoopusUtils.h"

#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/ScalarEvolutionExpander.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Operator.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/UnrollLoop.h"

#include <limits>

using namespace llvm;

// The maximum size of the not unrolled/rolled loop
cl::opt<unsigned> MaxRolledLoopSize("maxloopsize", cl::desc("The maximum size of the NOT unrolled loop."), cl::Optional, cl::init(std::numeric_limits<unsigned>::max()));
// The maximum size of the unrolled loop
cl::opt<unsigned> MaxUnrolledLoopSize("maxunrollsize", cl::desc("The maximum size of the unrolled loop."), cl::Optional, cl::init(std::numeric_limits<unsigned>::max()));

//===- Unroll functions ---------------------------------------------------===//
unsigned HDLLoopUnroll::ggT(unsigned a, unsigned b) {
  while (b != 0) {
    unsigned tmp = a % b;
    a = b;
    b = tmp;
  }
  return a;
  #if 0
  if (a == 0) {
    return b;
  } else {
    while (b != 0) {
      if (a > b) {
        a = a - b;
      } else {
        b = b - a;
      }
    }
    return a;
  }
  #endif
}

const MDNode* HDLLoopUnroll::getLoopMetadata(const Loop *L, const std::string &MDName) {
  if (L == nullptr) { return nullptr; }

  const MDNode *LID = L->getLoopID();
  if (LID == nullptr) { return nullptr; }
  if (LID->getNumOperands() <= 1) { return nullptr; }
  if (LID->getOperand(0) != LID) { return nullptr; }

  // We do not have to check the first operand as that is the self-reference
  for (unsigned i = 1, e = LID->getNumOperands(); i < e; ++i) {
    const MDNode *LMD = dyn_cast<MDNode>(LID->getOperand(i).get());
    if (LMD == nullptr) { continue; }

    // Get the description of the current MDNode
    if (LMD->getNumOperands() == 0) { continue; }
    const MDString *MDDesc = dyn_cast<MDString>(LMD->getOperand(0).get());
    if (MDDesc == nullptr) { continue; }

    // Check if that is the requested MD
    if (MDDesc->getString().equals(MDName) == true) {
      return LMD;
    }
  }
  return nullptr;
}

unsigned HDLLoopUnroll::getUnrollPragmaValue(Loop *L) {
  if (L == nullptr) { return 0; }

  const MDNode *SPIR_LUC_MD = getLoopMetadata(L, Loopus::LoopMDStrings::OPENCL_UNROLL_COUNT);
  const MDNode *LLVM_LUC_MD = getLoopMetadata(L, Loopus::LoopMDStrings::LLVM_UNROLL_COUNT);
  const ConstantInt *SPIR_LUC = nullptr;
  const ConstantInt *LLVM_LUC = nullptr;

  // We prefer the Spir/OpenCL metadata over the LLVM ones
  bool SPIRSucceeded = false;
  if (SPIR_LUC_MD != nullptr) {
    if (SPIR_LUC_MD->getNumOperands() == 2) {
      const Metadata *UnrollFactorMD = SPIR_LUC_MD->getOperand(1).get();
      if (UnrollFactorMD != nullptr) {
        SPIR_LUC = mdconst::dyn_extract<ConstantInt>(UnrollFactorMD);
        SPIRSucceeded = true;
      }
    }
  }
  if ((SPIRSucceeded == false) && (LLVM_LUC_MD != nullptr)) {
    if (LLVM_LUC_MD->getNumOperands() == 2) {
      const Metadata *UnrollFactorMD = LLVM_LUC_MD->getOperand(1).get();
      if (UnrollFactorMD != nullptr) {
        LLVM_LUC = mdconst::dyn_extract<ConstantInt>(UnrollFactorMD);
      }
    }
  }
  if ((SPIR_LUC == nullptr) && (LLVM_LUC == nullptr)) {
    return 0;
  } else if ((SPIR_LUC != nullptr) && (LLVM_LUC == nullptr)) {
    return SPIR_LUC->getZExtValue();
  } else if ((SPIR_LUC == nullptr) && (LLVM_LUC != nullptr)) {
    return LLVM_LUC->getZExtValue();
  } else if ((SPIR_LUC != nullptr) && (LLVM_LUC != nullptr)) {
    const unsigned SPIR_LUC_UNS = SPIR_LUC->getZExtValue();
    const unsigned LLVM_LUC_UNS = LLVM_LUC->getZExtValue();
    if (SPIR_LUC_UNS != LLVM_LUC_UNS) {
      DEBUG(dbgs() << "Inconsistent SPIR and LLVM loop unroll count found!");
    }
    return SPIR_LUC_UNS;
  }
  return 0;
}

bool HDLLoopUnroll::hasDisablePragma(Loop *L) {
  if (L == nullptr) { return false; }
  if ((getLoopMetadata(L, Loopus::LoopMDStrings::LLVM_UNROLL_DISABLE) != nullptr)
   || (getLoopMetadata(L, Loopus::LoopMDStrings::LOOPUS_SHIFTREG_FULL) != nullptr)) {
    // If there is a metadata created by LLVM/Clang we do not want to unroll.
    // If the loop is annotated to be a shift register loop we also do not want
    // to unroll.
    return true;
  }
  const MDNode *OpenCLUnrollHint = getLoopMetadata(L,
      Loopus::LoopMDStrings::OPENCL_UNROLL_COUNT);
  if (OpenCLUnrollHint != nullptr) {
    if (OpenCLUnrollHint->getNumOperands() >= 2) {
      const Metadata *UnrollCountMD = OpenCLUnrollHint->getOperand(1).get();
      const ConstantInt *UnrollCount = mdconst::dyn_extract<ConstantInt>(UnrollCountMD);
      if (UnrollCount->isOne() == true) {
        return true;
      }
    }
  }
  return false;
}

bool HDLLoopUnroll::hasFullUnrollPragma(Loop *L) {
  if (L == nullptr) { return false; }
  if (getLoopMetadata(L, Loopus::LoopMDStrings::LLVM_UNROLL_FULL) != nullptr) {
    return true;
  }
  return false;
}

void HDLLoopUnroll::printSCEVType(const SCEV* const S) const {
  if (S == nullptr) { return; }

  if (isa<SCEVCastExpr>(S) == true) {
    if (isa<SCEVSignExtendExpr>(S) == true) {
      DEBUG(dbgs() << "CASTEXPR>SExt");
    } else if (isa<SCEVTruncateExpr>(S) == true) {
      DEBUG(dbgs() << "CASTEXPR>Trunc");
    } else if (isa<SCEVZeroExtendExpr>(S) == true) {
      DEBUG(dbgs() << "CASTEXPR>ZExt");
    } else {
      DEBUG(dbgs() << "CASTEXPR");
    }
  } else if (isa<SCEVConstant>(S) == true) {
    DEBUG(dbgs() << "CONSTANT");
  } else if (isa<SCEVCouldNotCompute>(S) == true) {
    DEBUG(dbgs() << "COULDNOTCOMPUTE");
  } else if (isa<SCEVNAryExpr>(S) == true) {
    if (isa<SCEVAddRecExpr>(S) == true) {
      DEBUG(dbgs() << "NARYEXPR>AddRec");
    } else if (isa<SCEVCommutativeExpr>(S) == true) {
      if (isa<SCEVAddExpr>(S) == true) {
        DEBUG(dbgs() << "NARYEXPR>CommtvExpr>AddExpr");
      } else if (isa<SCEVMulExpr>(S) == true) {
        DEBUG(dbgs() << "NARYEXPR>CommtvExpr>MulExpr");
      } else if (isa<SCEVSMaxExpr>(S) == true) {
        DEBUG(dbgs() << "NARYEXPR>CommtvExpr>SMaxExpr");
      } else if (isa<SCEVUMaxExpr>(S) == true) {
        DEBUG(dbgs() << "NARYEXPR>CommtvExpr>UMaxExpr");
      } else {
        DEBUG(dbgs() << "NARYEXPR>CommtvExpr");
      }
    } else {
      DEBUG(dbgs() << "NARYEXPR");
    }
  } else if (isa<SCEVUDivExpr>(S) == true) {
    DEBUG(dbgs() << "UDIVEXPR");
  } else if (isa<SCEVUnknown>(S) == true) {
    DEBUG(dbgs() << "UNKNOWN");
  } else {
    DEBUG(dbgs() << "SCEV");
  }
}

/// \brief Tries to read the required workgroup size from the metadata.
///
/// Tries to read the required workgroup size for the given kernel function
/// in the given dimension from the metadata. If the metadata does not exist
/// or cannot be read for any reasons \c 0 is returned.
unsigned HDLLoopUnroll::getRequiredWorkGroupSize(const Function &F, unsigned Dimension) {
  const ConstantInt *ReqdSizeCI = getRequiredWorkGroupSizeConst(F, Dimension);
  if (ReqdSizeCI != nullptr) {
    return ReqdSizeCI->getZExtValue();
  } else {
    return 0;
  }
}
/// \brief Tries to read the required workgroup size from the metadata.
///
/// Tries to read the required workgroup size for the given kernel function
/// in the given dimension from the metadata. If the metadata does not exist
/// or cannot be read for any reasons \c 0 is returned.
const ConstantInt* HDLLoopUnroll::getRequiredWorkGroupSizeConst(const Function &F, unsigned Dimension) {
  if (MDK->isKernel(&F) == false) {
    // The rerquired workgroupsize metadata is invalid on non-kernel functions
    return nullptr;
  }
  // There are only three dimensions
  if (Dimension > 2) { return nullptr; }

  const MDNode *KernelMDNode = MDK->getMDNodeForFunction(&F);
  if (KernelMDNode == nullptr) { return nullptr; }

  if (KernelMDNode->getNumOperands() <= 1) { return nullptr; }
  for (unsigned i = 0, e = KernelMDNode->getNumOperands(); i < e; ++i) {
    const MDNode *MDOp = dyn_cast<MDNode>(KernelMDNode->getOperand(i).get());
    if (MDOp == nullptr) { continue; }

    if (MDOp->getNumOperands() != 4) { continue; }
    if (isa<MDString>(MDOp->getOperand(0)) == false) { continue; }
    const MDString *MDOpDescription = dyn_cast<MDString>(MDOp->getOperand(0));
    if (MDOpDescription == nullptr) { continue; }
    if (MDOpDescription->getString().equals(
        Loopus::KernelMDStrings::SPIR_REQD_WGSIZE) == false) { continue; }

    // Now we found the metadata node containing the workgroups sizes
    const Metadata *ReqdSizeMD = MDOp->getOperand(1 + Dimension).get();
    if (ReqdSizeMD == nullptr) { continue; }
    const ConstantInt *ReqdSize = mdconst::dyn_extract<ConstantInt>(ReqdSizeMD);
    if (ReqdSize == nullptr) { continue; }

    return ReqdSize;
  }
  return nullptr;
}

/// \brief Splits the given SCEV expression \c ArrayOffset into a scaling
/// \brief and an offset concerning \c GIDOp.
bool HDLLoopUnroll::isValidArrayOffset(const SCEV* ArrayOffset,
    const SCEV* GIDOp, const SCEV** Offset, const SCEV** Scaling) {
  if ((ArrayOffset == nullptr) || (GIDOp == nullptr) || (Offset == nullptr)
   || (Scaling == nullptr)) {
    return false;
  }

  // The easy case... ;-)
  if (ArrayOffset == GIDOp) {
    *Offset = SE->getConstant(GIDOp->getType(), 0, true);
    *Scaling = SE->getConstant(GIDOp->getType(), 1, true);
    return true;
  }


  if (isa<SCEVMulExpr>(ArrayOffset) == true) {
    const SCEVMulExpr* ArrOffMul = dyn_cast<SCEVMulExpr>(ArrayOffset);
    SmallVector<const SCEV*, 4> GIDOps;
    SmallVector<const SCEV*, 16> MulOps;
    for (unsigned i = 0, e = ArrOffMul->getNumOperands(); i < e; ++i) {
      const SCEV* Op = ArrOffMul->getOperand(i);
      if (SE->hasOperand(Op, GIDOp) == true) {
        GIDOps.push_back(Op);
      } else {
        MulOps.push_back(Op);
      }
    }
    if (GIDOps.size() > 1) {
      // If there are more than one operands using the GID the GID is used at
      // quadratic in the expression
      *Offset = nullptr;
      *Scaling = nullptr;
      return false;
    } else {
      for (const SCEV* GIDTerm : GIDOps) {
        if (isValidArrayOffset(GIDTerm, GIDOp, Offset, Scaling) == false) {
          return false;
        }
      }
    }
    const SCEV* MulTerm = SE->getMulExpr(MulOps);
    *Offset = SE->getMulExpr(*Offset, MulTerm);
    *Scaling = SE->getMulExpr(*Scaling, MulTerm);
    return true;
  } else if (isa<SCEVAddExpr>(ArrayOffset) == true) {
    const SCEVAddExpr* ArrOffAdd = dyn_cast<SCEVAddExpr>(ArrayOffset);
    SmallVector<const SCEV*, 4> GIDOps;
    SmallVector<const SCEV*, 16> AddOps;
    for (unsigned i = 0, e = ArrOffAdd->getNumOperands(); i < e; ++i) {
      const SCEV* Op = ArrOffAdd->getOperand(i);
      if (SE->hasOperand(Op, GIDOp) == true) {
        GIDOps.push_back(Op);
      } else {
        AddOps.push_back(Op);
      }
    }
    for (const SCEV* GIDTerm : GIDOps) {
      if (isValidArrayOffset(GIDTerm, GIDOp, Offset, Scaling) == false) {
        return false;
      }
    }
    const SCEV* AddTerm = SE->getAddExpr(AddOps);
    *Offset = SE->getAddExpr(*Offset, AddTerm);
    return true;
  } else if (isa<SCEVUDivExpr>(ArrayOffset) == true) {
    const SCEVUDivExpr* ArrOffUDiv = dyn_cast<SCEVUDivExpr>(ArrayOffset);
    if (SE->hasOperand(ArrOffUDiv->getRHS(), GIDOp) == true) {
      *Offset = nullptr;
      *Scaling = nullptr;
      return false;
    }
    if (SE->hasOperand(ArrOffUDiv->getLHS(), GIDOp) == true) {
      if (isValidArrayOffset(ArrOffUDiv->getLHS(), GIDOp, Offset, Scaling) == true) {
        *Offset = SE->getUDivExactExpr(*Offset, ArrOffUDiv->getRHS());
        *Scaling = SE->getUDivExactExpr(*Scaling, ArrOffUDiv->getRHS());
        return true;
      } else {
        return false;
      }
    } else {
      *Offset = ArrayOffset;
      *Scaling = SE->getConstant(GIDOp->getType(), 0, true);
      return true;
    }
  } else if (isa<SCEVConstant>(ArrayOffset) == true) {
    *Offset = ArrayOffset;
    *Scaling = SE->getConstant(GIDOp->getType(), 0, true);
    return true;
  } else if (isa<SCEVUnknown>(ArrayOffset) == true) {
    if (SE->hasOperand(ArrayOffset, GIDOp) == true) {
      *Offset = SE->getConstant(GIDOp->getType(), 0, true);
      *Scaling = SE->getConstant(GIDOp->getType(), 1, true);
    } else {
      *Offset = ArrayOffset;
      *Scaling = SE->getConstant(GIDOp->getType(), 0, true);
    }
    return true;
  } else if (isa<SCEVAddRecExpr>(ArrayOffset) == true) {
    const SCEVAddRecExpr *ArrOffAddRec = dyn_cast<SCEVAddRecExpr>(ArrayOffset);
    if (SE->hasOperand(ArrOffAddRec->getOperand(1), GIDOp) == false) {
      return isValidArrayOffset(ArrOffAddRec->getOperand(0), GIDOp, Offset, Scaling);
    } else {
      *Offset = nullptr;
      *Scaling = nullptr;
      return false;
    }
  } else {
    if (SE->hasOperand(ArrayOffset, GIDOp) == false) {
      *Offset = ArrayOffset;
      *Scaling = SE->getConstant(GIDOp->getType(), 1, true);
      return true;
    } else {
      *Offset = nullptr;
      *Scaling = nullptr;
      return false;
    }
  }

  return false;
}

bool HDLLoopUnroll::dependsOnLoop(const SCEV *SCEVVal, const Loop *L) {
  if ((SCEVVal == nullptr) || (L == nullptr)) { return false; }

  SmallPtrSet<const SCEV*, 8> Visited;
  SmallVector<const SCEV*, 8> Worklist;
  Worklist.push_back(SCEVVal);
  while (Worklist.empty() == false) {
    const SCEV *CurSCEV = Worklist.pop_back_val();
    if (Visited.count(CurSCEV) == 1) { continue; }
    Visited.insert(CurSCEV);

    if (isa<SCEVAddRecExpr>(CurSCEV) == true) {
      const SCEVAddRecExpr *CurSCEVAddRec = dyn_cast<SCEVAddRecExpr>(CurSCEV);
      if (CurSCEVAddRec->getLoop() == L) {
        return true;
      }
    }
    if (isa<SCEVCastExpr>(CurSCEV) == true) {
      Worklist.push_back(dyn_cast<SCEVCastExpr>(CurSCEV)->getOperand());
    } else if (isa<SCEVNAryExpr>(CurSCEV) == true) {
      const SCEVNAryExpr *CurSCEVNAE = dyn_cast<SCEVNAryExpr>(CurSCEV);
      for (unsigned i = 0, e = CurSCEVNAE->getNumOperands(); i < e; ++i) {
        Worklist.push_back(CurSCEVNAE->getOperand(i));
      }
    } else if (isa<SCEVUDivExpr>(CurSCEV) == true) {
      Worklist.push_back(dyn_cast<SCEVUDivExpr>(CurSCEV)->getLHS());
      Worklist.push_back(dyn_cast<SCEVUDivExpr>(CurSCEV)->getRHS());
    }
  }
  return false;
}

int HDLLoopUnroll::computeLoopUnrollCount(const Loop *L, unsigned *LoopCnt) {
  if (L == nullptr) { return -1; }

  DEBUG(dbgs() << "=====>>> In loop " << L->getHeader()->getName() << ":\n");
  // Determine loop size
  const unsigned LoopSize = RE->getRessourceUsage(L);
  if (LoopSize > MaxRolledLoopSize) {
    DEBUG(dbgs() << "   ret: Skipping loop. Unroll analysis size threshold reached!\n");
    return 0;
  }

  struct UnrollFactorInfo {
    Value *MemGepInst;
    Instruction *MemInst;
    const SCEV *UnrollFactor;

    UnrollFactorInfo(void)
     : MemGepInst(nullptr), MemInst(nullptr), UnrollFactor(nullptr) {
    }
    UnrollFactorInfo(Instruction *MemAcc)
     : MemGepInst(nullptr), MemInst(MemAcc), UnrollFactor(nullptr) {
    }
    UnrollFactorInfo(Value *MemGep, Instruction *MemAcc, const SCEV *UnrollF)
     : MemGepInst(MemGep), MemInst(MemAcc), UnrollFactor(UnrollF) {
    }
  };
  // This vector contains restrictions to the unroll factor for loop.
  SmallVector<UnrollFactorInfo, 8> UnrollFactors;

  const Function* const F = L->getHeader()->getParent();

  // This map is used to replace all calls to get_local_size by their
  // corresponding constant if provided in the metadata
  ValueToValueMap SCEVReplacements;
  {
    SmallVector<Argument*, 4> PromArgsLSZ;
    if (APT->getPromotedArgumentsList(F,
        BuiltInFunctionCall::BuiltInFunction::BIF_GetLocalSize, PromArgsLSZ) > 0) {
      for (SmallVector<Argument*, 4>::iterator ARGIT = PromArgsLSZ.begin(),
          ARGEND = PromArgsLSZ.end(); ARGIT != ARGEND; ++ARGIT) {
        const Argument* LocSizeArg = *ARGIT;
        long UsedDim = APT->getCallArgForPromotedArgument(LocSizeArg);
        ConstantInt *ReqdLocSize =
          const_cast<ConstantInt*>(getRequiredWorkGroupSizeConst(*F, UsedDim));
        if (ReqdLocSize != nullptr) {
          SCEVReplacements[LocSizeArg] = ReqdLocSize;
        }
      }
    }
  }

  for (Loop::block_iterator BIT = L->block_begin(), BEND = L->block_end();
      BIT != BEND; ++BIT) {
    BasicBlock *BB = *BIT;
    for (BasicBlock::iterator INSIT = BB->begin(), INSEND = BB->end();
        INSIT != INSEND; ++INSIT) {
      Instruction* const I = &*INSIT;

      // This is the unroll info for the current mem access
      struct UnrollFactorInfo UFI(I);

      // GetElementPtrInst *OldMemGep = nullptr;
      GEPOperator *MemGep = nullptr;
      bool isLoadInst = false;
      unsigned AccessedAddrSpace = Loopus::OpenCLAddressSpaces::ADDRSPACE_GENERIC;
      if (isa<LoadInst>(I) == true) {
        LoadInst *LD = dyn_cast<LoadInst>(I);
        if (LD != nullptr) {
          UFI.MemGepInst = LD->getPointerOperand();
          MemGep = dyn_cast<GEPOperator>(LD->getPointerOperand());
          AccessedAddrSpace = LD->getPointerAddressSpace();
          isLoadInst = true;
          DEBUG(dbgs() << "Found load: " << *LD << " @" << LD << "\n");
        }
      } else if (isa<StoreInst>(I) == true) {
        StoreInst *SI = dyn_cast<StoreInst>(I);
        if (SI != nullptr) {
          UFI.MemGepInst = SI->getPointerOperand();
          MemGep = dyn_cast<GEPOperator>(SI->getPointerOperand());
          AccessedAddrSpace = SI->getPointerAddressSpace();
          isLoadInst = false;
          DEBUG(dbgs() << "Found store: " << *SI << " @" << SI << "\n");
        }
      } else {
        continue;
      }

      // Check if we are accessing private memory
      if ((AccessedAddrSpace == Loopus::OpenCLAddressSpaces::ADDRSPACE_PRIVATE)
       || (AccessedAddrSpace == Loopus::OpenCLAddressSpaces::ADDRSPACE_CONSTANT)
       || (AccessedAddrSpace == Loopus::OpenCLAddressSpaces::ADDRSPACE_GENERIC)) {
        // Accesses to private memory cannot overlap and therefore does not
        // expose any limits to loop unrolling
        DEBUG(dbgs() << "  cont: accessing non-shared memory\n");
        continue;
      } else {
        // This should be global and local memory
      }

      if (MemGep == nullptr) {
        // All kernels are accessing the same address so we do not want to
        // change the code
        UFI.UnrollFactor = SE->getConstant(UFI.MemGepInst->getType(), 0, false);
        UnrollFactors.push_back(UFI);
        DEBUG(dbgs() << "    uf: disabled\n");
        DEBUG(dbgs() << "  >>Memory access has loop-independent index!\n");
        continue;
      }

      const SCEV *MemGepSCEV = SE->getSCEV(MemGep);
      if (MemGepSCEV == nullptr) {
        UFI.UnrollFactor = SE->getConstant(UFI.MemGepInst->getType(), 0, false);
        UnrollFactors.push_back(UFI);
        DEBUG(dbgs() << "    uf: disabled\n");
        DEBUG(dbgs() << "  >>No SCEV object found for memory GEP instruction!\n");
        continue;
      }

      DEBUG(dbgs() << "  ==>>  " << *MemGep << " @" << MemGep << "\n"
                   << "   gep: " << *MemGepSCEV << " @" << MemGepSCEV << "\n"
                   << "        gepty="; printSCEVType(MemGepSCEV); dbgs() << "\n");
      if (isa<SCEVAddRecExpr>(MemGepSCEV) == false) {
        // UFI.UnrollFactor = SE->getConstant(UFI.MemGepInst->getType(), 0, false);
        // UnrollFactors.push_back(UFI);
        // TODO: We could possible remove the limitation here. If the load is
        // independent of the current loop (as it is no recurrence it obviously
        // is) we might unroll the loop and replace the load in all unrolled
        // blocks by the value of the original load (gvn pass?)
        DEBUG(dbgs() << "    uf: disabled\n");
        DEBUG(dbgs() << "  >>Could not compute recurrence for memory access address!\n");
        continue;
      }

      // Check if the mem instruction depends on the currently inspected loop
      if (dependsOnLoop(MemGepSCEV, L) == false) {
        DEBUG(dbgs() << "    uf: no-lim\n");
        DEBUG(dbgs() << "  >>Skipping memory instruction as not depending on current loop\n");
        continue;
      }


      const SCEVAddRecExpr *MemGepSARE = dyn_cast<SCEVAddRecExpr>(MemGepSCEV);
      if (MemGepSARE->getLoop() != L) {
        UFI.UnrollFactor = SE->getConstant(UFI.MemGepInst->getType(), 0, false);
        UnrollFactors.push_back(UFI);
        DEBUG(dbgs() << "    uf: disabled\n");
        DEBUG(dbgs() << "  >>Memory instruction depends on other loops first\n");
        continue;
      }

      // The first address the gep computes is the offset operand
      const SCEV *MemStartAddr = MemGepSARE->getOperand(0);
      DEBUG(dbgs() << "   mem: first=" << *MemStartAddr << " @" << MemStartAddr << "\n");
      if (isa<SCEVCouldNotCompute>(MemStartAddr) == true) {
        DEBUG(dbgs() << "  >>Could not determine valid value for start!\n");
        continue;
      }

      // Determine the loop stride (so the value change between two iterations)
      const SCEV* LoopStride = MemGepSARE->getOperand(1);
      if ((LoopStride == nullptr)
       || (isa<SCEVCouldNotCompute>(LoopStride) == true)) {
        UFI.UnrollFactor = SE->getConstant(UFI.MemGepInst->getType(), 0, false);
        UnrollFactors.push_back(UFI);
        DEBUG(dbgs() << "    uf: disabled\n");
        DEBUG(dbgs() << "  >>No usable SCEV for loop stride found!\n");
        continue;
      }
      DEBUG(dbgs() << "  lstd: stride=" << *LoopStride << " @" << LoopStride << "\n"
                   << "        stridety="; printSCEVType(LoopStride); dbgs() << "\n");


      // Loops of the following form
      // int idx = c1 + get_global_id(x) * c2;
      // for (int i = 0; i < c3; ++i) {
      //   ... arr[idx + i] ...
      // }
      // result in the following constants
      //  - #elements assigned to one workitem    = c2
      //  - #elements processed by one workitem   = c3
      //  - #unprocessed elements at front of arr = c1
      //
      // Where c2 and c3 are constants that might depend on the local size of
      // a workgroup. In that case the required workgroup size should be provided
      // to the compiler by using
      //     kernel __attribute__((reqd_work_group_size(X,Y,Z)))
      // in the source code. Then the call to get_local_size can be replaced by
      // the constant and a proper trip count can be determined.
      // If c2 and/or c3 does depend on any values that are only known at
      // runtime the compiler cannot determine the number of assigned and
      // processed elements and will not do much.
      // Moreover idx should depend on the global id of a workitem. If it does
      // not, the assigned elements do overlap and all workitems will process the
      // same elements.
      //
      //             +-----------------+           +-----------------+
      //             | processed elems |           | processed elems |
      //
      // +-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+...
      // |  S  |  S  |  P  |  P  |  P  |  U  |  U  |  P  |  P  |  P  |  U  |
      // |  X  |  X  | WI0 | WI0 | WI0 | WI0 | WI0 | WI1 | WI1 | WI1 | WI1 |
      // +-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+...
      //
      // |offset(c1) |       assigned elems        |       assigned elems
      // +-----------+-----------------------------+------------------------...
      //
      // => e.g.: get_local_size(x)=2, c1=2, c2=2, c3=1, c4=1, c5=1
      // Like this the processed and assigned elements for the workitems do not
      // overlap and it is ok to unroll the loop.

      // Contains all arguments on which the expression depends
      SmallVector<Argument*, 8> DependingIDArgs;
      { // First determine the dimension the get_global_id call uses
        SmallVector<Argument*, 8> PromArgsID;
        APT->getPromotedArgumentsList(F,
            BuiltInFunctionCall::BuiltInFunction::BIF_GetGlobalID, PromArgsID);
        if (MemGep->getPointerAddressSpace() ==
            Loopus::OpenCLAddressSpaces::ADDRSPACE_LOCAL) {
          // If we are accessing local memory the local id can also be used to
          // make accesses unique
          APT->getPromotedArgumentsList(F,
              BuiltInFunctionCall::BuiltInFunction::BIF_GetLocalID, PromArgsID);
        }
        if (PromArgsID.size() > 0) {
          for (SmallVector<Argument*, 8>::iterator ARGIT = PromArgsID.begin(),
              ARGEND = PromArgsID.end(); ARGIT != ARGEND; ++ARGIT) {
            Argument* const Arg = *ARGIT;
            const SCEV* SCEVArg = SE->getSCEV(Arg);
            if ((SCEVArg != nullptr)
             && (SE->hasOperand(MemStartAddr, SCEVArg) == true)) {
              DependingIDArgs.push_back(Arg);
            }
          } // End of detected argument loop
        }
      }

      // We might need them later...
      const int MemWidth = Loopus::HardwareModel::getHardwareModel().getGMemBusWidthInBytes();
      const SCEV* MemoryWidth = SE->getConstant(LoopStride->getType(), MemWidth, true);
      const SCEV* UnrollFactorByStride = SE->getUDivExpr(MemoryWidth, LoopStride);
      DEBUG(dbgs() << "   mem: bw=" << *MemoryWidth << " @" << MemoryWidth << "\n"
                   << "   off: bw-stride-uf=" << *UnrollFactorByStride << " @" << UnrollFactorByStride << "\n");

      if (isLoadInst == true) {
        // Processing load instruction. Those are not as dangerous as store
        // instructions as they normally do not modify the memory
        // Loads that do depend on any global id are unrolled according to their
        // stride.
        // Loads depending on a global id are unrolled according to the scaling
        // factor used for that id.

        if (DependingIDArgs.empty() == true) {
          // The load does not depend on any global ID. So it might be possibly
          // shared among several workitems as they might all load from the
          // same memory location.
          DEBUG(dbgs() << "    uf: no-lim\n");
          DEBUG(dbgs() << "  >>Load not depending on id\n");
          continue;
        } else {
          for (Argument *DependentArg : DependingIDArgs) {
            DEBUG(dbgs() << "  load: checks for globalid arg " << *DependentArg
                << " @" << DependentArg << "\n");
            const SCEV *GIDArgSCEV = SE->getSCEV(DependentArg);

            const SCEV* SCEVAddOffset = nullptr;
            const SCEV* SCEVMulScaling = nullptr;
            if (isValidArrayOffset(MemStartAddr, GIDArgSCEV, &SCEVAddOffset,
                  &SCEVMulScaling) == true) {
              if ((SCEVAddOffset == nullptr)
                  || (SCEVMulScaling == nullptr)
                  || (isa<SCEVAddRecExpr>(SCEVAddOffset) == true)
                  || (isa<SCEVAddRecExpr>(SCEVMulScaling) == true)
                  || (isa<SCEVCouldNotCompute>(SCEVAddOffset) == true)
                  || (isa<SCEVCouldNotCompute>(SCEVMulScaling) == true)) {
                continue;
              }
              DEBUG(dbgs() << "   off: addoff=" << *SCEVAddOffset << " @" << SCEVAddOffset << "\n"
                           << "        mulscal=" << *SCEVMulScaling << " @" << SCEVMulScaling << "\n");

              const SCEV *DiffStrideScaling = SE->getMinusSCEV(LoopStride, SCEVMulScaling);
              if (DiffStrideScaling->isZero() == true) {
                // Loop stride and ID scaling are equal so we do not want expose
                // any unroll limits
                DEBUG(dbgs() << "    uf: no-lim\n");
                DEBUG(dbgs() << "  >>Stride and scaling are equal!\n");
                continue;
              } else if (SE->isKnownPositive(DiffStrideScaling) == true) {
                // Stride > Scaling
                const SCEV *StridedUnrollFactor = SE->getUDivExpr(LoopStride, SCEVMulScaling);
                DEBUG(dbgs() << "   off: stride-scaled-uf=" << *StridedUnrollFactor << " @" << StridedUnrollFactor << "\n");
                UFI.UnrollFactor = StridedUnrollFactor;
                UnrollFactors.push_back(UFI);
                DEBUG(dbgs() << "    uf: stride-scaled-uf used\n");
                continue;
              // } else if (SE->isKnownNegative(DiffStrideScaling) == true) {
              } else {
                const SCEV *ScaledUnrollFactor = SE->getUDivExpr(SCEVMulScaling, LoopStride);
                DEBUG(dbgs() << "   off: scale-stride-uf=" << *ScaledUnrollFactor << " @" << ScaledUnrollFactor << "\n");
                UFI.UnrollFactor = ScaledUnrollFactor;
                UnrollFactors.push_back(UFI);
                DEBUG(dbgs() << "    uf: scale-stride-uf used\n");
              }
            } else {
              DEBUG(dbgs() << "Could not split array offset into offset and scaling!\n");
            }
          } // End of argument loop
          continue;
        }
      } else {
        // Found a store instruction
        // The last address is the address the gep computes if the loop finishes
        const SCEV *MemEndAddr = SE->getSCEVAtScope(MemGepSARE, L->getParentLoop());
        DEBUG(dbgs() << "   mem: last=" << *MemEndAddr << " @" << MemEndAddr << "\n");
        if (isa<SCEVCouldNotCompute>(MemEndAddr) == true) {
          DEBUG(dbgs() << "  >>Could not determine valid value for end!\n");
          continue;
        }
        // Now determine the offset between
        const SCEV *MemAddrDiff = SE->getMinusSCEV(MemEndAddr, MemStartAddr);
        // Note that MemStartAddr and MemEndAddr are include bounds so we have to
        // add the stride to determine the number of accessed bytes: the number of
        // bytes between the first and last load is computed by the difference but
        // as the last load access loads again as many bytes as indicated by the
        // stride the loop access diff+stride many bytes.
        MemAddrDiff = SE->getAddExpr(MemAddrDiff, LoopStride);
        DEBUG(dbgs() << "        diff=" << *MemAddrDiff << " @" << MemAddrDiff << "\n"
                     << "        diffty="; printSCEVType(MemAddrDiff); dbgs() << "\n");

        if (DependingIDArgs.empty() == true) {
          // The store does not depend on any global id so do not unroll the
          // loop at all
          UFI.UnrollFactor = SE->getConstant(UFI.MemGepInst->getType(), 0, false);
          UnrollFactors.push_back(UFI);
          DEBUG(dbgs() << "    uf: disabled\n");
          DEBUG(dbgs() << "  >>Does not depend on any global id. Disabled unrolling.\n");
          continue;
        }

        for (Argument *DependentArg : DependingIDArgs) {
          DEBUG(dbgs() << "   str: checks for id argument " << *DependentArg
              << " @" << DependentArg << "\n");
          const SCEV *GIDArgSCEV = SE->getSCEV(DependentArg);

          // Now we have to determine the number of assigned elements
          const SCEV* SCEVAddOffset = nullptr;
          const SCEV* SCEVMulScaling = nullptr;
          if (isValidArrayOffset(MemStartAddr, GIDArgSCEV, &SCEVAddOffset,
                &SCEVMulScaling) == false) {
            UFI.UnrollFactor = SE->getConstant(UFI.MemGepInst->getType(), 0, false);
            UnrollFactors.push_back(UFI);
            DEBUG(dbgs() << "    uf: disabled\n");
            DEBUG(dbgs() << "  >>Could not split array offset into offset and scaling!\n");
            continue;
          }

          if ((SCEVAddOffset == nullptr)
              || (SCEVMulScaling == nullptr)
              || (isa<SCEVAddRecExpr>(SCEVAddOffset) == true)
              || (isa<SCEVAddRecExpr>(SCEVMulScaling) == true)
              || (isa<SCEVCouldNotCompute>(SCEVAddOffset) == true)
              || (isa<SCEVCouldNotCompute>(SCEVMulScaling) == true)) {
            UFI.UnrollFactor = SE->getConstant(UFI.MemGepInst->getType(), 0, false);
            UnrollFactors.push_back(UFI);
            DEBUG(dbgs() << "    uf: disabled\n");
            DEBUG(dbgs() << "  >>Invalid offset or scaling computed\n");
            continue;
          }
          DEBUG(dbgs() << "   off: addoff=" << *SCEVAddOffset << " @" << SCEVAddOffset << "\n"
                       << "        mulscal=" << *SCEVMulScaling << " @" << SCEVMulScaling << "\n");

          if (SE->isKnownNonPositive(SCEVMulScaling) == true) {
            DEBUG(dbgs() << "    uf: no-lim\n");
            DEBUG(dbgs() << "  >>Scaling factor for array offset is not positive!\n");
            continue;
          }

          // Now check that the elements assigned to each workitem do not overlap
          const SCEV* DiffAssgnProcd = SE->getMinusSCEV(SCEVMulScaling, MemAddrDiff);
          if (SE->isKnownNonNegative(DiffAssgnProcd) == false) {
            // We could not show that the difference is always positive so try
            // to rewrite the differences and try again
            DEBUG(dbgs() << "  Rewriting diffs to determine non-overlapping ranges!\n");
            const SCEV *DiffAssgnProcdRew = DiffAssgnProcd;
            if (SCEVReplacements.empty() == false) {
              DiffAssgnProcdRew = SCEVParameterRewriter::rewrite(DiffAssgnProcd,
                  *SE, SCEVReplacements, true);
            }
            if (SE->isKnownNonNegative(DiffAssgnProcdRew) == false) {
              UFI.UnrollFactor = UnrollFactorByStride;
              UnrollFactors.push_back(UFI);
              DEBUG(dbgs() << "    uf: bw-stride-uf used\n");
              DEBUG(dbgs() << "  >>Number of assigned element for each workitem "
                  << "could not be proven to be greater than zero!\n");
              continue;
            }
          }

          const SCEV *ScaledUnrollFactor = SE->getUDivExpr(SCEVMulScaling, LoopStride);
          DEBUG(dbgs() << "   off: scale-stride-uf=" << *ScaledUnrollFactor << " @" << ScaledUnrollFactor << "\n");
          DEBUG(dbgs() << "    uf: scale-stride-uf used\n");
          UFI.UnrollFactor = ScaledUnrollFactor;
          UnrollFactors.push_back(UFI);
        } // End of argument loop
      }
    } // End of instruction loop
  } // End of basic block loop


  DEBUG(dbgs() << "Computing loop unroll factor:\n");
  // Now we have to make sure that the loop count is greater than the
  // computed unroll factor
  const SCEV* LBTC = SE->getBackedgeTakenCount(L);
  if ((LBTC != nullptr) && (isa<SCEVCouldNotCompute>(LBTC) == false)) {
    const SCEV *ConstOne = SE->getConstant(LBTC->getType(), 1);
    LBTC = SE->getAddExpr(LBTC, ConstOne);
    DEBUG(dbgs() << "  loop: count=" << *LBTC << " @" << LBTC << "\n");
    if (isa<SCEVConstant>(LBTC) == false) {
      if (SCEVReplacements.empty() == false) {
        LBTC = SCEVParameterRewriter::rewrite(LBTC, *SE, SCEVReplacements, true);
        // The getBackedgeTakenCount function returns a SCEV that has the
        // value ((Start-End) + (Stride-1) / Stride
        // As AFAIK ScalarEvolution only performs integer arithmetics and
        // cannot handle floating point stuff the mentioned value should be
        // equal to (Start-End)/Stride as (Stride-1)/Stride < 1 and therefor
        // -- treated as ints -- (Stride-1)/Stride = 0.
        // But all that is just a guess...
        // See ScalarEvolution.cpp:
        //  - getBackedgeTakenCount ln4479 / ln4480
        //  - getBackedgeTakenInfo ln4503 / ln4517
        //  - computeBakcedgeTakenCount ln4754 / ln4768
        //  - ComputeExitLimit ln4813 / ln4880
        //  - ComputeExitLimitFromCond ln4901 / ln4983
        //  - ComputeExitLimitFromICmp ln5006 / ln5075
        //  - HowManyLessThans ln7090 / ln7137
        //  - computeBECount ln7074 / ln7078
        // For strides that are not a power of (1, 2, 4, 8, 16,...) the
        // backedge taken count is not computable. That results from the
        // HowManyLessThans function that calls some overflow check
        // (doesIVOverflowOnLT, called in ln7116) that fails for any number
        // not power of 2. But I don't know how to repair that. I don't even
        // know if that code is wrong and seems like it is completly ok...
        DEBUG(dbgs() << "        countrew=" << *LBTC << " @" << LBTC << "\n");
      }
    }
    if ((LBTC == nullptr) || (isa<SCEVConstant>(LBTC) == false)) {
      DEBUG(dbgs() << "Cannot determine loop count (A)!\n");
      return 0;
    }
  } else {
    DEBUG(dbgs() << "Cannot determine loop count (B)!\n");
    return 0;
  }

  // Now check if we can completly unroll the loop. Therefore all detected
  // unroll factors must be greater than the loop count.
  if (isa<SCEVConstant>(LBTC) == false) {
    return 0;
  }

  const unsigned LoopCount = dyn_cast<SCEVConstant>(LBTC)->getValue()->getZExtValue();
  if (LoopCnt != nullptr) {
    *LoopCnt = LoopCount;
  }

  // Now determine the maximum unroll count depending on the loop size and the
  // given threshold
  const unsigned ThresholdUnrollFactor = MaxUnrolledLoopSize / LoopSize;
  DEBUG(dbgs() << "        sz=" << LoopSize << "\n"
               << "        szth=" << MaxUnrolledLoopSize << "\n"
               << "        thuf=" << ThresholdUnrollFactor << "\n");

  // There are no limiting memory accesses in the loop
  if (UnrollFactors.size() == 0) {
    if (LoopCount > ThresholdUnrollFactor) {
      DEBUG(dbgs() << "   ret: Size threshold reached. Limiting unroll factor "
                   << "to " << ThresholdUnrollFactor << "!\n");
      return ThresholdUnrollFactor;
    } else {
      return LoopCount;
    }
  }

  // First determine the GGT for all collected unroll factors
  bool allGreaterThanCount = true;
  bool hasZero = false;
  for (const struct UnrollFactorInfo &CurUFI : UnrollFactors) {
    if (CurUFI.UnrollFactor->isZero() == true) {
      hasZero = true;
      break;
    }
    if (isa<SCEVConstant>(CurUFI.UnrollFactor) == true) {
      const unsigned CurUnrollF = dyn_cast<SCEVConstant>(CurUFI.UnrollFactor)
          ->getValue()->getZExtValue();
      if (CurUnrollF < LoopCount) {
        allGreaterThanCount = false;
      }
    }
  }
  if (hasZero == true) {
    // There is at least one load/store/memory access that limits the unroll
    // factor to zero (and like this forbids unrolling)
    DEBUG(dbgs() << "   ret: at least one load/store restricting unrolling to zero\n");
    return 0;
  }
  if (allGreaterThanCount == true) {
    // Each memory access could be unrolled more often than the loop executes
    DEBUG(dbgs() << "   ret: all greater than loop count\n" );
    if (LoopCount > ThresholdUnrollFactor) {
      DEBUG(dbgs() << "   ret: Size threshold reached. Limiting unroll factor "
                   << "to " << ThresholdUnrollFactor << "!\n");
      return ThresholdUnrollFactor;
    } else {
      return LoopCount;
    }
  }

  unsigned AlloverUnrollF = 0;
  AlloverUnrollF = dyn_cast<SCEVConstant>(UnrollFactors[0].UnrollFactor)->getValue()->getZExtValue();
  for (const struct UnrollFactorInfo &CurUFI : UnrollFactors) {
    if (isa<SCEVConstant>(CurUFI.UnrollFactor) == true) {
      const unsigned CurUnrollF = dyn_cast<SCEVConstant>(CurUFI.UnrollFactor)
          ->getValue()->getZExtValue();
      AlloverUnrollF = ggT(AlloverUnrollF, CurUnrollF);
    }
  }
  DEBUG(dbgs() << "        ffactor=" << AlloverUnrollF << "\n");

  if (AlloverUnrollF > ThresholdUnrollFactor) {
    DEBUG(dbgs() << "   ret: Size threshold reached. Limiting unroll factor "
                 << "to " << ThresholdUnrollFactor << "!\n");
    return ThresholdUnrollFactor;
  } else {
    return AlloverUnrollF;
  }
}

void HDLLoopUnroll::setLoopUnrolledMD(Loop *L) {
  if (L == nullptr) { return; }

  MDNode *LoopMD = L->getLoopID();
  // First collect all metadata already attached to the loop
  SmallVector<Metadata*, 4> LoopMDs;
  // Reserve the first metadata op for the loop id itself (self-ref)
  LoopMDs.push_back(nullptr);

  if (LoopMD != nullptr) {
    for (unsigned i = 1, e = LoopMD->getNumOperands(); i < e; ++i) {
      bool isUnrollMD = false;
      Metadata *MDOp = LoopMD->getOperand(i);
      if (MDOp != nullptr) {
        MDString *MDOpStr = dyn_cast<MDString>(MDOp);
        if (MDOpStr != nullptr) {
          const StringRef MDStr = MDOpStr->getString();
          if ((MDStr.startswith_lower("llvm.loop.unroll") == true)
           || (MDStr.equals(Loopus::LoopMDStrings::OPENCL_UNROLL_COUNT) == true)) {
            isUnrollMD = true;
          }
        }
      }
      if (isUnrollMD == false) {
        LoopMDs.push_back(MDOp);
      }
    }
  }

  // Now create the disable metadata
  LLVMContext &Context = L->getHeader()->getContext();
  MDString *DisableUnroll = MDString::get(Context,
      Loopus::LoopMDStrings::LLVM_UNROLL_DISABLE);
  SmallVector<Metadata*, 1> DisableUnrollMDVec;
  DisableUnrollMDVec.push_back(DisableUnroll);
  MDNode *DisableUnrollMD = MDNode::get(Context, DisableUnrollMDVec);
  LoopMDs.push_back(DisableUnrollMD);
  MDNode *NewLoopID = MDNode::get(Context, LoopMDs);
  NewLoopID->replaceOperandWith(0, NewLoopID);
  L->setLoopID(NewLoopID);
}

bool HDLLoopUnroll::handleLoop(Loop *L, LPPassManager &LPM) {
  if (L == nullptr) { return false; }

  // Now check if we want to unroll the current loop
  if (hasDisablePragma(L) == true) {
    return false;
  }

  unsigned LoopCount = 0;
  unsigned UnrollFactor = computeLoopUnrollCount(L, &LoopCount);
  const unsigned PragmaUnrollFactor = getUnrollPragmaValue(L);
  const bool FullUnrolling = hasFullUnrollPragma(L);
  if (LoopCount == 0) {
    LoopCount = SE->getSmallConstantTripCount(L);
    if (LoopCount == 0) {
      return false;
    }
  }

  // If there is a pragma perfer it
  if (PragmaUnrollFactor != 0) {
    UnrollFactor = PragmaUnrollFactor;
  } else if (FullUnrolling == true) {
    UnrollFactor = LoopCount;
  }
  if (UnrollFactor > LoopCount) {
    UnrollFactor = LoopCount;
  }
  if (UnrollFactor == 0) {
    return false;
  }
  if (UnrollFactor == LoopCount) {
    DEBUG(dbgs() << "  loop: full-unrolling enabled!\n");
  }

  // Currently we do not perform any runtime unrolling
  unsigned LoopTripMultiple = 1;
  BasicBlock *ExitingBlock = L->getLoopLatch();
  if (ExitingBlock != nullptr) {
    LoopTripMultiple = SE->getSmallConstantTripMultiple(L, ExitingBlock);
  }
  setLoopUnrolledMD(L);
  bool Changed = UnrollLoop(L, UnrollFactor, LoopCount, false, LoopTripMultiple,
      LI, this, &LPM, AC);

  return Changed;
}

//===- Implementation of LLVM pass ----------------------------------------===//
INITIALIZE_PASS_BEGIN(HDLLoopUnroll, "loopus-loopunroll", "Unroll loops",  true, false)
INITIALIZE_PASS_DEPENDENCY(ArgPromotionTracker)
INITIALIZE_PASS_DEPENDENCY(AssumptionCacheTracker)
INITIALIZE_PASS_DEPENDENCY(DataLayoutPass)
INITIALIZE_PASS_DEPENDENCY(LoopInfo)
INITIALIZE_PASS_DEPENDENCY(OpenCLMDKernels)
INITIALIZE_PASS_DEPENDENCY(ScalarEvolution)
INITIALIZE_PASS_END(HDLLoopUnroll, "loopus-loopunroll", "Unroll loops",  true, false)

char HDLLoopUnroll::ID = 0;

namespace llvm {
  Pass* createHDLLoopUnrollPass() {
    return new HDLLoopUnroll();
  }
}

HDLLoopUnroll::HDLLoopUnroll(void)
 : LoopPass(ID), AC(nullptr), APT(nullptr), DL(nullptr), LI(nullptr),
   MDK(nullptr), RE(nullptr), SE(nullptr) {
  initializeHDLLoopUnrollPass(*PassRegistry::getPassRegistry());
  RE = new Loopus::SimpleRessourceEstimator();
}

HDLLoopUnroll::~HDLLoopUnroll(void) {
  delete RE;
}

void HDLLoopUnroll::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<ArgPromotionTracker>();
  AU.addPreserved<ArgPromotionTracker>();
  AU.addRequired<AssumptionCacheTracker>();
  AU.addRequired<DataLayoutPass>();
  AU.addPreserved<DataLayoutPass>();
  AU.addRequired<LoopInfo>();
  AU.addRequired<OpenCLMDKernels>();
  AU.addPreserved<OpenCLMDKernels>();
  AU.addRequired<ScalarEvolution>();
}

bool HDLLoopUnroll::runOnLoop(Loop *L, LPPassManager &LPM) {
  AC = &getAnalysis<AssumptionCacheTracker>().getAssumptionCache(
      *L->getHeader()->getParent());
  APT = &getAnalysis<ArgPromotionTracker>();
  DL = &getAnalysis<DataLayoutPass>().getDataLayout();
  LI = &getAnalysis<LoopInfo>();
  MDK = &getAnalysis<OpenCLMDKernels>();
  SE = &getAnalysis<ScalarEvolution>();

  if ((AC == nullptr) || (APT == nullptr) || (DL == nullptr)
   || (LI == nullptr) || (MDK == nullptr) || (SE == nullptr)) {
    return false;
  }

  bool Changed = handleLoop(L, LPM);
  return Changed;
}
