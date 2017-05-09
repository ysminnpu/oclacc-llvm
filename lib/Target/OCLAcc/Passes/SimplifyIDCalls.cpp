//===- SimplifyIDCalls.cpp ------------------------------------------------===//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "loopus-simplifyid"

#include "SimplifyIDCalls.h"

#include "LoopusUtils.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CallingConv.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <map>
#include <string>

using namespace llvm;

//===- Implementation of helper functions ---------------------------------===//
cl::opt<bool> NoNullifyGlobalOffset("nonullgloboff", cl::desc("Do not subsitute the global_work_offset with null/zero."), cl::Optional, cl::init(false));

STATISTIC(StatsNumCSsReplacedConst, "Number of callsites replaced by constants");
STATISTIC(StatsNumArgssReplacedConst, "Number of arguments replaced by constants");


ConstantInt* SimplifyIDCalls::getRequiredWorkGroupSizeConst(
    const Function *F, unsigned Dimension) const {
  if (MDK->isKernel(F) == false) {
    // The rerquired workgroupsize metadata is invalid on non-kernel functions
    return nullptr;
  }

  const MDNode *KernelMDNode = MDK->getMDNodeForFunction(F);
  if (KernelMDNode == nullptr) { return nullptr; }

  if (KernelMDNode->getNumOperands() <= 1) { return nullptr; }
  for (unsigned i = 1, e = KernelMDNode->getNumOperands(); i < e; ++i) {
    const MDNode *MDOp = dyn_cast<MDNode>(KernelMDNode->getOperand(i).get());
    if (MDOp == nullptr) { continue; }

    if (MDOp->getNumOperands() < 2 + Dimension) { continue; }
    if (isa<MDString>(MDOp->getOperand(0)) == false) { continue; }
    const MDString *MDOpDescription = dyn_cast<MDString>(MDOp->getOperand(0));
    if (MDOpDescription == nullptr) { continue; }
    if (MDOpDescription->getString().equals(
        Loopus::KernelMDStrings::SPIR_REQD_WGSIZE) == false) { continue; }

    // Now we found the metadata node containing the workgroups sizes
    const Metadata *ReqdSizeMD = MDOp->getOperand(1 + Dimension).get();
    if (ReqdSizeMD == nullptr) { continue; }
    ConstantInt *ReqdSize = mdconst::dyn_extract<ConstantInt>(ReqdSizeMD);
    if (ReqdSize == nullptr) { continue; }

    return ReqdSize;
  }
  return nullptr;
}

/// \brief Tries to reduce the number of calls to workitem builtin functions.
///
/// As the result of the workitem functions (like get_local_size,...) does not
/// change during runtime the function tries to reduce the number of calls to
/// those functions
bool SimplifyIDCalls::reduceIDCalls(Function *F) {
  if (F == nullptr) { return false; }

  bool Changed = false;
  typedef std::pair<std::string, const ConstantInt*> MapKeyTy;
  std::map<MapKeyTy, Instruction*> BIFCallMap;

  for (inst_iterator INSIT = inst_begin(F), INSEND = inst_end(F);
      INSIT != INSEND; ) {
    // To avoid invalidation of iterator as we might erase the instruction
    Instruction *I = &*INSIT;
    ++INSIT;

    if (isa<CallInst>(I) == false) { continue; }
    CallInst *CI = dyn_cast<CallInst>(I);
    const Function *CalledF = CI->getCalledFunction();
    if (CalledF == nullptr) { continue; }

    // Check that we are calling a workitem function
    const std::string CalledFName = CalledF->getName();
    if (ocl::NameMangling::isWorkItemFunction(CalledFName) == false) { continue; }
    // Check that the argument is a constant
    MapKeyTy BIFKey;
    const unsigned NumArgs = CI->getNumArgOperands();
    if (NumArgs == 0) {
      BIFKey = std::make_pair(CalledFName, nullptr);
    } else if (NumArgs == 1) {
      const Value *ArgVal = CI->getArgOperand(0);
      if (isa<ConstantInt>(ArgVal) == false) { continue; }
      BIFKey = std::make_pair(CalledFName, dyn_cast<ConstantInt>(ArgVal));
    } else {
      // There is no workitem function that accepts more than one argument
      BIFKey = std::make_pair("", nullptr);
      continue;
    }

    if (BIFCallMap.count(BIFKey) == 0) {
      // No entry found so this should be the first time that we use this call
      // The call must be moved to a location where it dominates all its uses
      // and in the header it should do so... (and that should be faster than
      // creating computing dominator tree)
      CI->moveBefore(F->getEntryBlock().getFirstInsertionPt());
      BIFCallMap[BIFKey] = CI;
    } else {
      // There is already an entry so replace this call by the stored call (that
      // should already be in the proper location).
      Instruction *OldI = BIFCallMap[BIFKey];
      if (OldI != nullptr) {
        CI->replaceAllUsesWith(OldI);
        CI->eraseFromParent();
        Changed = true;
      }
    }
  }

  return Changed;
}

bool SimplifyIDCalls::replaceConstIDCalls(Function *F) {
  if (F == nullptr) { return false; }

  bool Changed = false;
  for (inst_iterator INSIT = inst_begin(F), INSEND = inst_end(F);
      INSIT != INSEND; ) {
    Instruction *I = &*INSIT;
    ++INSIT;

    if (isa<CallInst>(I) == false) { continue; }
    CallInst *CI = dyn_cast<CallInst>(I);

    const Function *CalledF = CI->getCalledFunction();
    if (CalledF == nullptr) { continue; }
    const std::string CalledFName = CalledF->getName().str();

    if (CalledFName.compare(ocl::NameMangling::mangleName("get_local_size")) == 0) {
      if (CI->getNumArgOperands() != 1) { continue; }
      const ConstantInt *CIArg = dyn_cast<ConstantInt>(CI->getArgOperand(0));
      if (CIArg == nullptr) { continue; }
      const unsigned CIArgDim = CIArg->getZExtValue();
      ConstantInt *CIResult = getRequiredWorkGroupSizeConst(F, CIArgDim);
      if (CIResult == nullptr) { continue; }
      CI->replaceAllUsesWith(CIResult);
      CI->eraseFromParent();
      Changed = true;
      ++StatsNumCSsReplacedConst;

    } else if ((NoNullifyGlobalOffset == false)
     && (CalledFName.compare(ocl::NameMangling::mangleName("get_global_offset")) == 0)) {
      if (CI->getNumArgOperands() != 0) { continue; }
      Constant *ConstZero = ConstantInt::get(
          CalledF->getFunctionType()->getReturnType(), 0, false);
      if (ConstZero == nullptr) { continue; }
      CI->replaceAllUsesWith(ConstZero);
      CI->eraseFromParent();
      Changed = true;
      ++StatsNumCSsReplacedConst;
    } else {
      continue;
    }
  }

  return Changed;
}

bool SimplifyIDCalls::replaceConstIDArgs(Function *F) {
  if (APT->empty() == true) { return false; }

  bool Changed = false;
  for (Argument &A : F->args()) {
    if (APT->isPromotedArgument(&A) == false) { continue; }

    const auto BIFCallTarget = APT->getBIFForPromotedArgument(&A);
    if (BIFCallTarget == BuiltInFunctionCall::BuiltInFunction::BIF_GetLocalSize) {
      const unsigned CIArgDim = static_cast<unsigned>(APT->getCallArgForPromotedArgument(&A));
      ConstantInt *CIResult = getRequiredWorkGroupSizeConst(F, CIArgDim);
      A.replaceAllUsesWith(CIResult);
      Changed = true;
    } else if ((NoNullifyGlobalOffset == false)
     && (BIFCallTarget == BuiltInFunctionCall::BuiltInFunction::BIF_GetGlobalOffset)) {
     Constant *ConstZero = ConstantInt::get(A.getType(), 0, false);
     A.replaceAllUsesWith(ConstZero);
     Changed = true;
    }
  }

  ++StatsNumArgssReplacedConst;

  return Changed;
}

bool SimplifyIDCalls::replaceAggregateIDCalls(Function *F, Module *M) {
  if (F == nullptr) { return false; }
  if (M == nullptr) { return false; }
  if (F->getParent() != M) { return false; }

  // If the aggregate ID functions are used multiple times we reuse the
  // computations. The pointers hold the instruction that compute the final
  // result.
  Instruction *LocLinIDResult = nullptr;
  Instruction *GlobLinIDResult = nullptr;

  bool Changed = false;
  for (inst_iterator INSIT = inst_begin(F), INSEND = inst_end(F);
      INSIT != INSEND; ) {
    Instruction *I = &*INSIT;
    ++INSIT;

    if (isa<CallInst>(I) == false) { continue; }
    CallInst *CI = dyn_cast<CallInst>(I);
    const Function *CalledF = CI->getCalledFunction();
    if (CalledF == nullptr) { continue; }
    const std::string CalledFName = CalledF->getName();

    // const std::string GetGlobIDMFN = ocl::NameMangling::mangleName("get_global_id");

    const std::string GetLocLinIDMFN = ocl::NameMangling::mangleName("get_local_linear_id");
    const std::string GetGlobLinIDMFN = ocl::NameMangling::mangleName("get_global_linear_id");

    if ((CalledFName.compare(GetLocLinIDMFN) != 0)
     && (CalledFName.compare(GetGlobLinIDMFN) != 0)) {
      continue;
    }
    if (CI->getNumArgOperands() != 0) { continue; }

    // Now we can inject the computation for the calls
    if (CalledFName.compare(GetLocLinIDMFN) == 0) {

      if (LocLinIDResult == nullptr) {
        // We need these functions: get_local_id, get_local_size
        const std::string GetLocIDMFN = ocl::NameMangling::mangleName("get_local_id");
        const std::string GetLocSizeMFN = ocl::NameMangling::mangleName("get_local_size");

        Function *GetLocIDF = M->getFunction(GetLocIDMFN);
        Function *GetLocSizeF = M->getFunction(GetLocSizeMFN);
        if ((GetLocIDF == nullptr) || (GetLocSizeF == nullptr)) {
          // At least one function is missing so create the missing functions
          // The return type of the functions is (according to OpenCL 2.0 spec)
          // size_t. As we do not known the exact width of it we use the return
          // type of the called function as it is size_t, too.
          Type *NewRetTy = CalledF->getFunctionType()->getReturnType();
          // Both functions expect one argument with type uint. According to specs
          // it is an 32 bit integer.
          Type *NewArgTy = IntegerType::get(getGlobalContext(), 32);
          SmallVector<Type*, 1> NewFArgs; NewFArgs.push_back(NewArgTy);
          // Create missing functions
          FunctionType *NewFTy = FunctionType::get(NewRetTy, NewFArgs, false);
          if (GetLocIDF == nullptr) {
            GetLocIDF = dyn_cast<Function>(M->getOrInsertFunction(GetLocIDMFN, NewFTy));
            if (GetLocIDF == nullptr) { continue; }
            Changed = true;
            // Add attributes
            GetLocIDF->setAttributes(CalledF->getAttributes());
            GetLocIDF->setCallingConv(CallingConv::SPIR_FUNC);
          }
          if (GetLocSizeF == nullptr) {
            GetLocSizeF = dyn_cast<Function>(M->getOrInsertFunction(GetLocSizeMFN, NewFTy));
            if (GetLocSizeF == nullptr) { continue; }
            Changed = true;
            // Add attributes
            GetLocSizeF->setAttributes(CalledF->getAttributes());
            GetLocSizeF->setCallingConv(CallingConv::SPIR_FUNC);
          }
        } // End of if to check for existing functions

        // Now we can make the computation explicit. We do also use the formula
        // for the general 3-dimensional case: in 2D or 1D cases the formula also
        // computes the correct results as the get_local_id and get_local_size
        // functions return proper default values if they are called with arguments
        // that are greater than the number of dimensions.
        // We will also insert the computation instructions in the header block of
        // the function so that we can reuse them without having to care about any
        // domination issues.
        BasicBlock *InsertBB = &F->getEntryBlock();
        // First create call instructions and the therefore needed constants
        Constant *ConstArgZero = ConstantInt::get(
            GetLocIDF->getFunctionType()->getParamType(0), 0, false);
        Constant *ConstArgOne = ConstantInt::get(
            GetLocIDF->getFunctionType()->getParamType(0), 1, false);
        Constant *ConstArgTwo = ConstantInt::get(
            GetLocIDF->getFunctionType()->getParamType(0), 2, false);
        SmallVector<Value*, 1> ArgsZero; ArgsZero.push_back(ConstArgZero);
        SmallVector<Value*, 1> ArgsOne; ArgsOne.push_back(ConstArgOne);
        SmallVector<Value*, 1> ArgsTwo; ArgsTwo.push_back(ConstArgTwo);

        CallInst *LocIDZeroCI = CallInst::Create(GetLocIDF, ArgsZero);
        LocIDZeroCI->setAttributes(CI->getAttributes());
        LocIDZeroCI->setDebugLoc(CI->getDebugLoc());
        CallInst *LocIDOneCI = CallInst::Create(GetLocIDF, ArgsOne);
        LocIDOneCI->setAttributes(CI->getAttributes());
        LocIDOneCI->setDebugLoc(CI->getDebugLoc());
        CallInst *LocIDTwoCI = CallInst::Create(GetLocIDF, ArgsTwo);
        LocIDTwoCI->setAttributes(CI->getAttributes());
        LocIDTwoCI->setDebugLoc(CI->getDebugLoc());
        CallInst *LocSizeZeroCI = CallInst::Create(GetLocSizeF, ArgsZero);
        LocSizeZeroCI->setAttributes(CI->getAttributes());
        LocSizeZeroCI->setDebugLoc(CI->getDebugLoc());
        CallInst *LocSizeOneCI = CallInst::Create(GetLocSizeF, ArgsOne);
        LocSizeOneCI->setAttributes(CI->getAttributes());
        LocSizeOneCI->setDebugLoc(CI->getDebugLoc());

        BinaryOperator *Sz1Sz0 = BinaryOperator::Create(Instruction::BinaryOps::Mul,
            LocSizeOneCI, LocSizeZeroCI);
        Sz1Sz0->setDebugLoc(CI->getDebugLoc());
        BinaryOperator *Id1Sz0 = BinaryOperator::Create(Instruction::BinaryOps::Mul,
            LocIDOneCI, LocSizeZeroCI);
        Id1Sz0->setDebugLoc(CI->getDebugLoc());
        BinaryOperator *Id2Szs = BinaryOperator::Create(Instruction::BinaryOps::Mul,
            LocIDTwoCI, Sz1Sz0);
        Id2Szs->setDebugLoc(CI->getDebugLoc());
        BinaryOperator *SumDim01 = BinaryOperator::Create(Instruction::BinaryOps::Add,
            Id1Sz0, LocIDZeroCI);
        SumDim01->setDebugLoc(CI->getDebugLoc());
        BinaryOperator *SumDims = BinaryOperator::Create(Instruction::BinaryOps::Add,
            Id2Szs, SumDim01, "explicit_local_linear_id");
        SumDims->setDebugLoc(CI->getDebugLoc());

        // Now inject the instructions
        LocIDZeroCI->insertBefore(InsertBB->getFirstInsertionPt());
        LocIDOneCI->insertAfter(LocIDZeroCI);
        LocIDTwoCI->insertAfter(LocIDOneCI);
        LocSizeZeroCI->insertAfter(LocIDTwoCI);
        LocSizeOneCI->insertAfter(LocSizeZeroCI);
        Sz1Sz0->insertAfter(LocSizeOneCI);
        Id1Sz0->insertAfter(Sz1Sz0);
        Id2Szs->insertAfter(Id1Sz0);
        SumDim01->insertAfter(Id2Szs);
        SumDims->insertAfter(SumDim01);
        Changed = true;

        // Remember the final instruction
        LocLinIDResult = SumDims;
      } // End of if to check for existing computation
      if (LocLinIDResult != nullptr) {
        CI->replaceAllUsesWith(LocLinIDResult);
        CI->eraseFromParent();
        Changed = true;
      }

    } else if (CalledFName.compare(GetGlobLinIDMFN) == 0) {
      if (GlobLinIDResult == nullptr) {
        // We need these functions: get_global_id, get_global_size, get_global_offset
        const std::string GetGlobIDMFN = ocl::NameMangling::mangleName("get_global_id");
        const std::string GetGlobSizeMFN = ocl::NameMangling::mangleName("get_global_size");
        const std::string GetGlobOffMFN = ocl::NameMangling::mangleName("get_global_offset");

        Function *GetGlobIDF = M->getFunction(GetGlobIDMFN);
        Function *GetGlobSizeF = M->getFunction(GetGlobSizeMFN);
        Function *GetGlobOffF = M->getFunction(GetGlobOffMFN);
        if ((GetGlobIDF == nullptr) || (GetGlobSizeF == nullptr)
         || (GetGlobOffF == nullptr)) {
          // At least one function is missing so create the missing functions
          // The return type of the functions is (according to OpenCL 2.0 spec)
          // size_t. As we do not known the exact width of it we use the return
          // type of the called function as it is size_t, too.
          Type *NewRetTy = CalledF->getFunctionType()->getReturnType();
          // Both functions expect one argument with type uint. According to specs
          // it is an 32 bit integer.
          Type *NewArgTy = IntegerType::get(getGlobalContext(), 32);
          SmallVector<Type*, 1> NewFArgs; NewFArgs.push_back(NewArgTy);
          // Create missing functions
          FunctionType *NewFTy = FunctionType::get(NewRetTy, NewFArgs, false);
          if (GetGlobIDF == nullptr) {
            GetGlobIDF = dyn_cast<Function>(M->getOrInsertFunction(GetGlobIDMFN, NewFTy));
            if (GetGlobIDF == nullptr) { continue; }
            Changed = true;
            // Add attributes
            GetGlobIDF->setAttributes(CalledF->getAttributes());
            GetGlobIDF->setCallingConv(CallingConv::SPIR_FUNC);
          }
          if (GetGlobSizeF == nullptr) {
            GetGlobSizeF = dyn_cast<Function>(M->getOrInsertFunction(GetGlobSizeMFN, NewFTy));
            if (GetGlobSizeF == nullptr) { continue; }
            Changed = true;
            // Add attributes
            GetGlobSizeF->setAttributes(CalledF->getAttributes());
            GetGlobSizeF->setCallingConv(CallingConv::SPIR_FUNC);
          }
          if ((GetGlobOffF == nullptr) && (NoNullifyGlobalOffset == true)) {
            GetGlobOffF = dyn_cast<Function>(M->getOrInsertFunction(GetGlobOffMFN, NewFTy));
            if (GetGlobOffF == nullptr) { continue; }
            Changed = true;
            // Add attributes
            GetGlobOffF->setAttributes(CalledF->getAttributes());
            GetGlobOffF->setCallingConv(CallingConv::SPIR_FUNC);
          } else {
            GetGlobOffF = nullptr;
          }
        } // End of if to check for existing functions

        // Now we can make the computation explicit. We do also use the formula
        // for the general 3-dimensional case: in 2D or 1D cases the formula also
        // computes the correct results as the get_local_id and get_local_size
        // functions return proper default values if they are called with arguments
        // that are greater than the number of dimensions.
        // We will also insert the computation instructions in the header block of
        // the function so that we can reuse them without having to care about any
        // domination issues.
        BasicBlock *InsertBB = &F->getEntryBlock();
        // First create call instructions and the therefore needed constants
        Constant *ConstArgZero = ConstantInt::get(
            GetGlobIDF->getFunctionType()->getParamType(0), 0, false);
        Constant *ConstArgOne = ConstantInt::get(
            GetGlobIDF->getFunctionType()->getParamType(0), 1, false);
        Constant *ConstArgTwo = ConstantInt::get(
            GetGlobIDF->getFunctionType()->getParamType(0), 2, false);
        SmallVector<Value*, 1> ArgsZero; ArgsZero.push_back(ConstArgZero);
        SmallVector<Value*, 1> ArgsOne; ArgsOne.push_back(ConstArgOne);
        SmallVector<Value*, 1> ArgsTwo; ArgsTwo.push_back(ConstArgTwo);

        CallInst *GlobIDZeroCI = CallInst::Create(GetGlobIDF, ArgsZero);
        GlobIDZeroCI->setAttributes(CI->getAttributes());
        GlobIDZeroCI->setDebugLoc(CI->getDebugLoc());
        CallInst *GlobIDOneCI = CallInst::Create(GetGlobIDF, ArgsOne);
        GlobIDOneCI->setAttributes(CI->getAttributes());
        GlobIDOneCI->setDebugLoc(CI->getDebugLoc());
        CallInst *GlobIDTwoCI = CallInst::Create(GetGlobIDF, ArgsTwo);
        GlobIDTwoCI->setAttributes(CI->getAttributes());
        GlobIDTwoCI->setDebugLoc(CI->getDebugLoc());
        CallInst *GlobOffZeroCI = nullptr;
        CallInst *GlobOffOneCI = nullptr;
        CallInst *GlobOffTwoCI = nullptr;
        if (NoNullifyGlobalOffset == true) {
          GlobOffZeroCI = CallInst::Create(GetGlobOffF, ArgsZero);
          GlobOffZeroCI->setAttributes(CI->getAttributes());
          GlobOffZeroCI->setDebugLoc(CI->getDebugLoc());
          GlobOffOneCI = CallInst::Create(GetGlobOffF, ArgsOne);
          GlobOffOneCI->setAttributes(CI->getAttributes());
          GlobOffOneCI->setDebugLoc(CI->getDebugLoc());
          GlobOffTwoCI = CallInst::Create(GetGlobOffF, ArgsTwo);
          GlobOffTwoCI->setAttributes(CI->getAttributes());
          GlobOffTwoCI->setDebugLoc(CI->getDebugLoc());
        }
        CallInst *GlobSizeZeroCI = CallInst::Create(GetGlobSizeF, ArgsZero);
        GlobSizeZeroCI->setAttributes(CI->getAttributes());
        GlobSizeZeroCI->setDebugLoc(CI->getDebugLoc());
        CallInst *GlobSizeOneCI = CallInst::Create(GetGlobSizeF, ArgsOne);
        GlobSizeOneCI->setAttributes(CI->getAttributes());
        GlobSizeOneCI->setDebugLoc(CI->getDebugLoc());

        Instruction *IdDif0 = nullptr;
        Instruction *IdDif1 = nullptr;
        Instruction *IdDif2 = nullptr;
        if (NoNullifyGlobalOffset == false) {
          IdDif0 = GlobIDZeroCI;
          IdDif1 = GlobIDOneCI;
          IdDif2 = GlobIDTwoCI;
        } else {
          IdDif0 = BinaryOperator::Create(Instruction::BinaryOps::Sub,
              GlobIDZeroCI, GlobOffZeroCI);
          IdDif0->setDebugLoc(CI->getDebugLoc());
          IdDif1 = BinaryOperator::Create(Instruction::BinaryOps::Sub,
              GlobIDOneCI, GlobOffOneCI);
          IdDif2->setDebugLoc(CI->getDebugLoc());
          IdDif2 = BinaryOperator::Create(Instruction::BinaryOps::Sub,
              GlobIDTwoCI, GlobOffTwoCI);
          IdDif2->setDebugLoc(CI->getDebugLoc());
        }
        BinaryOperator *Sz1Sz0 = BinaryOperator::Create(Instruction::BinaryOps::Mul,
            GlobSizeOneCI, GlobSizeZeroCI);
        Sz1Sz0->setDebugLoc(CI->getDebugLoc());
        BinaryOperator *Id2Szs = BinaryOperator::Create(Instruction::BinaryOps::Mul,
            IdDif2, Sz1Sz0);
        Id2Szs->setDebugLoc(CI->getDebugLoc());
        BinaryOperator *Id1Sz0 = BinaryOperator::Create(Instruction::BinaryOps::Mul,
            IdDif1, GlobSizeZeroCI);
        Id1Sz0->setDebugLoc(CI->getDebugLoc());
        BinaryOperator *TmpSum = BinaryOperator::Create(Instruction::BinaryOps::Add,
            Id2Szs, Id1Sz0);
        TmpSum->setDebugLoc(CI->getDebugLoc());
        BinaryOperator *SumDims = BinaryOperator::Create(Instruction::BinaryOps::Add,
            TmpSum, IdDif0, "explicit_global_linear_id");
        SumDims->setDebugLoc(CI->getDebugLoc());

        // Now inject the instructions
        GlobIDZeroCI->insertBefore(InsertBB->getFirstInsertionPt());
        GlobIDOneCI->insertAfter(GlobIDZeroCI);
        GlobIDTwoCI->insertAfter(GlobIDOneCI);
        GlobSizeZeroCI->insertAfter(GlobIDTwoCI);
        GlobSizeOneCI->insertAfter(GlobSizeZeroCI);
        if (NoNullifyGlobalOffset == true) {
          GlobOffZeroCI->insertAfter(GlobSizeOneCI);
          GlobOffOneCI->insertAfter(GlobOffZeroCI);
          GlobOffTwoCI->insertAfter(GlobOffOneCI);
          IdDif0->insertAfter(GlobOffTwoCI);
          IdDif1->insertAfter(IdDif0);
          IdDif2->insertAfter(IdDif1);
          Sz1Sz0->insertAfter(IdDif2);
        } else {
          Sz1Sz0->insertAfter(GlobSizeOneCI);
        }
        Id2Szs->insertAfter(Sz1Sz0);
        Id1Sz0->insertAfter(Id2Szs);
        TmpSum->insertAfter(Id1Sz0);
        SumDims->insertAfter(TmpSum);
        Changed = true;

        // Remember the final instruction
        GlobLinIDResult = SumDims;
      }
      if (GlobLinIDResult != nullptr) {
        CI->replaceAllUsesWith(GlobLinIDResult);
        CI->eraseFromParent();
        Changed = true;
      }
    } else {
      continue;
    }
  }

  return Changed;
}

bool SimplifyIDCalls::handleFunction(Function *F, Module *M) {
  if (F == nullptr) { return false; }
  if (M == nullptr) { return false; }
  if (F->getParent() != M) { return false; }

  bool Changed = false;
  Changed |= replaceAggregateIDCalls(F, M);
  Changed |= reduceIDCalls(F);
  Changed |= replaceConstIDCalls(F);
  Changed |= replaceConstIDArgs(F);
  return Changed;
}

//===- Implementation of LLVM pass ----------------------------------------===//
INITIALIZE_PASS_BEGIN(SimplifyIDCalls, "loopus-simplifyid", "Replace ID calls",  false, false)
INITIALIZE_PASS_DEPENDENCY(ArgPromotionTracker)
INITIALIZE_PASS_DEPENDENCY(OpenCLMDKernels)
INITIALIZE_PASS_END(SimplifyIDCalls, "loopus-simplifyid", "Replace ID calls",  true, false)

char SimplifyIDCalls::ID = 0;

namespace llvm {
  Pass* createSimplifyIDCallsPass() {
    return new SimplifyIDCalls();
  }
}

SimplifyIDCalls::SimplifyIDCalls(void)
 : ModulePass(ID), APT(nullptr), MDK(nullptr) {
  initializeSimplifyIDCallsPass(*PassRegistry::getPassRegistry());
}

void SimplifyIDCalls::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<ArgPromotionTracker>();
  AU.addRequired<OpenCLMDKernels>();
  AU.addPreserved<OpenCLMDKernels>();
  AU.setPreservesCFG();
}
// The pass has to be a module pass as the replaceAggregate function might
// create new functions.
bool SimplifyIDCalls::runOnModule(Module &M) {
  APT = &getAnalysis<ArgPromotionTracker>();
  MDK = &getAnalysis<OpenCLMDKernels>();
  if ((APT == nullptr) || (MDK == nullptr)) {
    return false;
  }

  bool Changed = false;
  for (Function &F : M.functions()) {
    if (F.isDeclaration() == true) { continue; }
    Changed |= handleFunction(&F, &M);
  }
  return Changed;

}

