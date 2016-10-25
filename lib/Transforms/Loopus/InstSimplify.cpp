//===- InstSimplify.cpp ---------------------------------------------------===//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "instsimplify"

#include "InstSimplify.h"

#include "LoopusUtils.h"

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

STATISTIC(StatsNumInstsTransformed, "Number of instructions simplified/transformed");

//===- Helper functions ---------------------------------------------------===//
bool InstSimplify::handleInst(Instruction *I) {
  if (I == nullptr) { return false; }

  const unsigned Opcode = I->getOpcode();
  if (Opcode == Instruction::BinaryOps::Mul) {
    const Value *MulOp = I->getOperand(1);
    if (MulOp == nullptr) { return false; }
    BinaryOperator *BOI = dyn_cast<BinaryOperator>(I);
    // Check if the operand is a power of 2
    const ConstantInt *MulOpCI = dyn_cast<ConstantInt>(MulOp);
    if (MulOpCI == nullptr) { return false; }
    if (MulOpCI->getValue().isPowerOf2() == true) {
      // So we have a int-mul with a constant that is a power of 2. So let's
      // replace it by a left-shift
      const unsigned ShiftLen = MulOpCI->getValue().logBase2();
      ConstantInt *ShiftLenCI = ConstantInt::get(MulOpCI->getType(), ShiftLen,
          false);
      BinaryOperator *ShiftI = BinaryOperator::Create(Instruction::BinaryOps::Shl,
          I->getOperand(0), ShiftLenCI);
      ShiftI->insertBefore(I);
      ShiftI->setName(I->getName());
      ShiftI->setDebugLoc(I->getDebugLoc());
      ShiftI->setHasNoSignedWrap(BOI->hasNoSignedWrap());
      ShiftI->setHasNoUnsignedWrap(BOI->hasNoUnsignedWrap());
      I->replaceAllUsesWith(ShiftI);
      Loopus::eraseFromParentRecursively(I);
      ++StatsNumInstsTransformed;
      return true;
    }
  } else if (Opcode == Instruction::BinaryOps::Add) {
    Value *AddOp0 = I->getOperand(0);
    const Value *AddOp1 = I->getOperand(1);
    if ((AddOp0 == nullptr) || (AddOp1 == nullptr)) { return false; }
    BinaryOperator *BOI = dyn_cast<BinaryOperator>(I);
    if (AddOp0 == AddOp1) {
      // This is a multiplication with two and therefore a left-shift by one
      Constant *ShiftLenCI = ConstantInt::get(AddOp0->getType(), 1,
          false);
      BinaryOperator *ShiftI = BinaryOperator::Create(Instruction::BinaryOps::Shl,
          AddOp0, ShiftLenCI);
      ShiftI->insertBefore(I);
      ShiftI->setName(I->getName());
      ShiftI->setDebugLoc(I->getDebugLoc());
      ShiftI->setHasNoSignedWrap(BOI->hasNoSignedWrap());
      ShiftI->setHasNoUnsignedWrap(BOI->hasNoUnsignedWrap());
      I->replaceAllUsesWith(ShiftI);
      Loopus::eraseFromParentRecursively(I);
      ++StatsNumInstsTransformed;
      return true;
    }
  }

  return false;
}

//===- Implementation of LLVM pass ----------------------------------------===//
INITIALIZE_PASS_BEGIN(InstSimplify, "loopus-instsimplify", "Simplify instructions", false, false)
INITIALIZE_PASS_END(InstSimplify, "loopus-instsimplify", "Simplify instructions", false, false)

char InstSimplify::ID = 0;

namespace llvm {
  Pass* createInstSimplifyPass() {
    return new InstSimplify();
  }
}

InstSimplify::InstSimplify(void)
 : FunctionPass(ID) {
}

void InstSimplify::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesCFG();
}

bool InstSimplify::runOnFunction(Function &F) {
  bool Changed = false;

  for (inst_iterator INSIT = inst_begin(F), INSEND = inst_end(F);
      INSIT != INSEND; ) {
    Instruction *I = &*INSIT;
    ++INSIT;

    Changed |= handleInst(I);
  }

  return Changed;
}
