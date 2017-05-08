//===- RewriteExpr.cpp ----------------------------------------------------===//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "loopus-rewexpr"

#include "RewriteExpr.h"

#include "LoopusUtils.h"

#include "llvm/ADT/Twine.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <utility>

using namespace llvm;

STATISTIC(StatsNumExprRewritten, "Number of rewritten expressions");
STATISTIC(StatsNumMathExprRewritten, "Number of replaced math expressions");

// Defines the maximum number of times that one single operand may occur in an
// add expression. If it is used more times than allowed by this offset a multiply
// expression is created and used as operand in the add expression.
cl::opt<int> MaxOpOccursAddExpr("addopthreshold",
    cl::desc("Maximum number of times that one operand may occur in an add expression."),
    cl::Optional, cl::init(3));
// Enable detection and simplification of mathematical expressions (especially
// of trigonometric functions).
cl::opt<bool> EnableMathSimplify("enablemathsimplify",
    cl::desc("Enable simplification of mathematical expressions and terms."),
    cl::Optional, cl::init(false));

//===- Expression functions -----------------------------------------------===//
BinaryOperator* RewriteExpr::canRewriteOp(Value *V, const unsigned Opcode,
    const unsigned NumUses) {
  if (V == nullptr) { return nullptr; }
  if (isa<BinaryOperator>(V) == false) { return nullptr; }
  BinaryOperator *BI = dyn_cast<BinaryOperator>(V);
  if (BI == nullptr) { return nullptr; }
  if (BI->getOpcode() != Opcode) { return nullptr; }
  if (BI->getNumUses() != NumUses) { return nullptr; }
  return BI;
}

bool RewriteExpr::collectOperands(Instruction *I, LeafMapTy &Leaves) {
  if (I == nullptr) { return false; }
  if (isa<BinaryOperator>(I) == false) { return false; }
  if ((I->isAssociative() == false) || (I->isCommutative() == false)) { return false; }

  DEBUG(dbgs() << "Collecting ops for: " << *I << "\n");
  const unsigned BitWidth = I->getType()->getScalarType()->getPrimitiveSizeInBits();
  const unsigned OPC = I->getOpcode();

  // We are collecting all operands that are used in the current expression.
  SmallVector<std::pair<Value*, APInt>, 8> Worklist;

  // This map stores all leaf operands that we currently found
  // std::map<Value*, APInt> Leaves;
  SmallPtrSet<Value*, 16> Visited;

  const APInt APONE(BitWidth, 1, true);
  for (unsigned i = 0, e = I->getNumOperands(); i < e; ++i) {
    Worklist.push_back(std::make_pair(I->getOperand(i), APONE));
  }

  while (Worklist.empty() == false) {
    std::pair<Value*, APInt> ValPair = Worklist.pop_back_val();
    Value *ValOp = ValPair.first;
    Visited.insert(ValOp);

    BinaryOperator *BinOp = canRewriteOp(ValOp, OPC, 1);
    if (BinOp != nullptr) {
      // The taken operand can be rewritten so its operands are inserted into
      // the worklist
      DEBUG(dbgs() << "  FOLLOWING Op: " << *BinOp << "\n");
      for (unsigned i = 0, e = BinOp->getNumOperands(); i < e; ++i) {
        Worklist.push_back(std::make_pair(BinOp->getOperand(i), ValPair.second));
      }
      continue;
    } else {
      // The processed operand seems to be a leaf node so now we have to add an
      // entry into the leaf map. The entry represents the number of times the
      // node occures in the final expression. Now there are several cases:
      if (Leaves.count(ValOp) <= 0) {
        // The current op has not been seen before so add it to the leaf map.
        // At this point the op is either not of the correct operation (wrong
        // opcode) or it is used multiple times that we did not all see yet...
        Leaves[ValOp] = ValPair.second;
        if (ValOp->hasOneUse() == false) {
          DEBUG(dbgs() << "  ILEAF: " << *ValOp << "\n");
          continue;
        } else {
          // Try morphing the operand
          DEBUG(dbgs() << "  MORPHTRY: " << *ValOp << "\n");
        }
      } else {
        // The op was already seen
        const APInt LeafCount = Leaves[ValOp] + ValPair.second;
        if (LeafCount.sge(ValOp->getNumUses()) == true) {
          // The op is only used in the expression. So if it has the proper
          // opcode we might try to rewrite its operands.
          BinaryOperator *MultiBinOp = canRewriteOp(ValOp, OPC, ValOp->getNumUses());
          Leaves[ValOp] = LeafCount;
          if (MultiBinOp != nullptr) {
            DEBUG(dbgs() << "  FOLLOWING Leaf: " << *MultiBinOp << "\n");
            for (unsigned i = 0, e = MultiBinOp->getNumOperands(); i < e; ++i) {
              // We want to process the operands of op to make the expression
              // larger
              Worklist.push_back(std::make_pair(MultiBinOp->getOperand(i), LeafCount));
            }
            Leaves.erase(ValOp);
            continue;
          } else {
            // Try morphing the operand
            DEBUG(dbgs() << "  MORPHTRY: " << *ValOp << "\n");
          }
        } else {
          // The op is also used outside the expression so it may not be rewritten
          DEBUG(dbgs() << "  FLEAF (" << LeafCount << "/" << ValOp->getNumUses()
              << "): " << *ValOp << "\n");
          Leaves[ValOp] = LeafCount;
          continue;
        }
      }
    }

    // Now we know that the operand may be rewritten
    Instruction *ValIns = dyn_cast<Instruction>(ValOp);
    if (ValIns == nullptr) { continue; }

    // Now we might try to morph the processed operand
    if (OPC == Instruction::BinaryOps::Add) {
      if (ValIns->getOpcode() == Instruction::BinaryOps::Mul) {
        // Check for the mul case: A + B + ... + (42 * X) + ...
        // In that case we will take X as operand that should be added 42 times.
        ConstantInt *ConstMulOp = nullptr;
        Value *ValMulOp = nullptr;
        if (isa<ConstantInt>(ValIns->getOperand(0)) == true) {
          ConstMulOp = dyn_cast<ConstantInt>(ValIns->getOperand(0));
          ValMulOp = ValIns->getOperand(1);
        } else if (isa<ConstantInt>(ValIns->getOperand(1)) == true) {
          ConstMulOp = dyn_cast<ConstantInt>(ValIns->getOperand(1));
          ValMulOp = ValIns->getOperand(0);
        }
        if ((ValMulOp != nullptr) && (ConstMulOp != nullptr)) {
          // There is a constant mul operand so the non-const operand can be
          // inserted into the worklist
          DEBUG(dbgs() << "  MORPH: mul with const found. Forwarding val op\n");
          const APInt WKCount = ValPair.second * ConstMulOp->getValue();
          Worklist.push_back(std::make_pair(ValMulOp, WKCount));
          Leaves.erase(ValOp);
        }
      }
    }

  } // End of worklist loop

  DEBUG(
    dbgs() << "Leaves:\n";
    for (LeafMapTy::const_iterator LFIT = Leaves.cbegin(),
        LFEND = Leaves.cend(); LFIT != LFEND; ++LFIT) {
      dbgs() << " - " << *LFIT->first << " : " << LFIT->second << "\n";
    }
  );

  return false;
}

bool RewriteExpr::rewriteExpression(LeafMapTy &Leaves, Instruction *I) {
  if (Leaves.empty() == true) { return false; }
  if (I == nullptr) { return false; }
  bool Changed = false;

  const Instruction::BinaryOps Opcode = static_cast<Instruction::BinaryOps>(I->getOpcode());

  typedef SmallVector<Value*, 16> WLTy;
  WLTy Worklist;

  // First we fill the worklist. We aggregate all seen constants on the fly. So
  // in the end there should only be one single constant operand. Each other
  // operand that is used multiple times is inserted several times into the
  // worklist (one exception: for add-instructions a mul-operation is created if
  // the threshold is reached).
  Constant *aggConst = nullptr;
  const Constant* ConstIdentity = ConstantExpr::getBinOpIdentity(Opcode, I->getType());
  for (LeafMapTy::iterator LFIT = Leaves.begin(), LFEND = Leaves.end();
      LFIT != LFEND; ++LFIT) {
    if (LFIT->second == 0) { continue; }
    const int Occurences = LFIT->second.getSExtValue();
    if ((ConstIdentity != nullptr) && (LFIT->first == ConstIdentity)) {
      // This is the identity for the processed operation. So it can be skipped...
      continue;
    }

    if (isa<Constant>(LFIT->first) == true) {
      // Merge all constants
      Constant* const curConst = dyn_cast<Constant>(LFIT->first);
      Constant *curAggConst = curConst;
      for (int i = 1; i < Occurences; ++i) {
        curAggConst = ConstantExpr::get(Opcode, curAggConst, curConst);
      }
      if (aggConst == nullptr) {
        aggConst = curAggConst;
      } else {
        aggConst = ConstantExpr::get(Opcode, aggConst, curAggConst);
      }
      continue;
    } else {
      // Processing non-const
      if (Opcode == Instruction::BinaryOps::Add) {
        if (Occurences > MaxOpOccursAddExpr) {
          // We replace all the operands by one mul instruction
          Value* const MulOpFirst = LFIT->first;
          Constant *Factor = ConstantInt::get(MulOpFirst->getType(), Occurences, true);
          BinaryOperator *MulOp = BinaryOperator::Create(Instruction::BinaryOps::Mul,
              MulOpFirst, Factor);
          if (isa<Argument>(MulOpFirst) == true) {
            BasicBlock *FuncEntryBB = &(dyn_cast<Argument>(MulOpFirst)->getParent()->getEntryBlock());
            Instruction *InsertIPt = &*FuncEntryBB->getFirstInsertionPt();
            MulOp->insertBefore(InsertIPt);
            Changed = true;
          } else if (isa<Instruction>(MulOpFirst) == true) {
            if (isa<PHINode>(MulOpFirst) == true) {
              BasicBlock *InstBB = dyn_cast<Instruction>(MulOpFirst)->getParent();
              Instruction *InsertIPt = &*InstBB->getFirstInsertionPt();
              MulOp->insertBefore(InsertIPt);
            } else {
              MulOp->insertAfter(dyn_cast<Instruction>(MulOpFirst));
            }
            // MulOp->insertAfter(dyn_cast<Instruction>(MulOpFirst));
            Changed = true;
          } else {
            llvm_unreachable("Hmm...?! What are you doing...?!");
          }
          Worklist.push_back(MulOp);
        } else {
          // Several adds should be created later
          for (int i = 0; i < Occurences; ++i) {
            Worklist.push_back(LFIT->first);
          }
        }
      } else {
        for (int i = 0; i < Occurences; ++i) {
          Worklist.push_back(LFIT->first);
        }
      }
    }
  }
  // Now add the aggregated constant
  if (aggConst != nullptr) {
    Worklist.push_back(aggConst);
  }
  
  Constant* ConstAbsorber = ConstantExpr::getBinOpAbsorber(Opcode, I->getType());
  if (ConstAbsorber != nullptr) {
    for (Value *Val : Worklist) {
      if (Val == ConstAbsorber) {
        Worklist.clear();
        Worklist.push_back(ConstAbsorber);
        break;
      }
    }
  }

  DEBUG(
    dbgs() << "Worklist contents:\n";
    for (WLTy::iterator WLIT = Worklist.begin(), WLEND = Worklist.end();
        WLIT != WLEND; ++WLIT) {
      dbgs() << " - " << **WLIT << "\n";
    }
  );

  // Now we want to insert new instructions to compute the expression but in a#
  // way that it takes only log time to get the result. So we really want a tree.
  // For 1xA, 1xB, 2xC we want something like
  // %add.00 = add %A, %B
  // %add.01 = add %C, %C
  // %add.02 = add %add.00, %add.01
  // In hardware the first two adds can be computed in parallel.

  if (Worklist.size() == 0) {
    // Hmm... What to do now?!?!
    return Changed;
  }

  WLTy Worklist2;
  // The pointers to the worklist for the current level in the computation tree
  // and to the next/higher level where we insert the new created instructions.
  // When all instructions on the current level are processed we do only have to
  // swap the pointers.
  WLTy *curLvlWL = &Worklist; WLTy *nextLvlWL = &Worklist2;
  while (true) {
    while (curLvlWL->size() > 1) {
      Value *Op1 = curLvlWL->pop_back_val();
      Value *Op2 = curLvlWL->pop_back_val();
      BinaryOperator *NewBO = BinaryOperator::Create(Opcode, Op1, Op2);
      Changed = true;
      nextLvlWL->push_back(NewBO);

      // Now we are trying to insert the new instruction as high as possible in
      // the dominator tree.
      Instruction *OpI1 = dyn_cast<Instruction>(Op1);
      Instruction *OpI2 = dyn_cast<Instruction>(Op2);
      if ((OpI1 != nullptr) && (OpI2 != nullptr)) {
        if (OpI1 != OpI2) {
          if (DT->dominates(OpI1, OpI2) == true) {
            if (isa<PHINode>(OpI2) == true) {
              Instruction *InsertIPt = &*OpI2->getParent()->getFirstInsertionPt();
              NewBO->insertBefore(InsertIPt);
            } else {
              NewBO->insertAfter(OpI2);
            }
          } else if (DT->dominates(OpI2, OpI1) == true) {
            if (isa<PHINode>(OpI1) == true) {
              Instruction *InsertIPt = &*OpI1->getParent()->getFirstInsertionPt();
              NewBO->insertBefore(InsertIPt);
            } else {
              NewBO->insertAfter(OpI1);
            }
          } else {
            llvm_unreachable("No domination order available!");
          }
        } else {
          if (isa<PHINode>(OpI1) == true) {
            Instruction *InsertIPt = &*OpI1->getParent()->getFirstInsertionPt();
            NewBO->insertBefore(InsertIPt);
          } else {
            NewBO->insertAfter(OpI1);
          }
        }
      } else if ((OpI1 != nullptr) && (OpI2 == nullptr)) {
        if (isa<PHINode>(OpI1) == true) {
          Instruction *InsertIPt = &*OpI1->getParent()->getFirstInsertionPt();
          NewBO->insertBefore(InsertIPt);
        } else {
          NewBO->insertAfter(OpI1);
        }
      } else if ((OpI1 == nullptr) && (OpI2 != nullptr)) {
        if (isa<PHINode>(OpI2) == true) {
          Instruction *InsertIPt = &*OpI2->getParent()->getFirstInsertionPt();
          NewBO->insertBefore(InsertIPt);
        } else {
          NewBO->insertAfter(OpI2);
        }
      } else {
        NewBO->insertBefore(I);
      }
      DEBUG(dbgs() << "  New binop: " << *NewBO << "\n");
    }
    assert(curLvlWL->size() <= 1 && "Worklist still contains more than one element.");
    if (curLvlWL->size() == 1) {
      if (nextLvlWL->size() > 0) {
        // There is something to do on the next level in the computation tree
        // so we are not finished yet...
        using std::swap;
        nextLvlWL->push_back(curLvlWL->pop_back_val());
        swap(curLvlWL, nextLvlWL);
      } else {
        // I think we are done here...
        break;
      }
    } else {
      // Current level does not contain any elements. So just swap the lists. If
      // there is only one instruction on the next level that will be handled in
      // the next iteration.
      using std::swap;
      swap(curLvlWL, nextLvlWL);
    }
  }
  assert(curLvlWL->size() == 1 && "Current level does not contain any computations.");

  Value *FinalValue = curLvlWL->back();
  FinalValue->setName(I->getName());
  if (isa<Instruction>(FinalValue) == true) {
    cast<Instruction>(FinalValue)->setDebugLoc(I->getDebugLoc());
  }
  I->replaceAllUsesWith(FinalValue);
  DEBUG(dbgs() << "  Replaced: " << *I << "\n    by " << *FinalValue << "\n");
  Loopus::eraseFromParentRecursively(I);

  ++StatsNumExprRewritten;

  return Changed;
}

bool RewriteExpr::canBreakSub(Instruction *I) {
  if (I == nullptr) { return false; }
  if ((I->getOpcode() != Instruction::BinaryOps::Sub)
   && (I->getOpcode() != Instruction::BinaryOps::FSub)) {
    return false;
  }

  if ((BinaryOperator::isNeg(I) == true)
   || (BinaryOperator::isFNeg(I) == true)) {
    return false;
  }

  Value *Op0 = I->getOperand(0);
  Value *Op1 = I->getOperand(1);
  // If one of the operands is a sub or an add and is only used by this
  // instruction then we should split it up into a negation.
  if ((canRewriteOp(Op0, Instruction::BinaryOps::Add) != nullptr)
   || (canRewriteOp(Op0, Instruction::BinaryOps::FAdd) != nullptr)
   || (canRewriteOp(Op0, Instruction::BinaryOps::Sub) != nullptr)
   || (canRewriteOp(Op0, Instruction::BinaryOps::FSub) != nullptr)) {
    return true;
  }
  if ((canRewriteOp(Op1, Instruction::BinaryOps::Add) != nullptr)
   || (canRewriteOp(Op1, Instruction::BinaryOps::FAdd) != nullptr)
   || (canRewriteOp(Op1, Instruction::BinaryOps::Sub) != nullptr)
   || (canRewriteOp(Op1, Instruction::BinaryOps::FSub) != nullptr)) {
    return true;
  }
  // If we have only one user which is an add then we should rewrite this sub
  // into a negation.
  if (I->hasOneUse() == true) {
    User *Usr = I->user_back();
    if ((canRewriteOp(Usr, Instruction::BinaryOps::Add) != nullptr)
     || (canRewriteOp(Usr, Instruction::BinaryOps::FAdd) != nullptr)
     || (canRewriteOp(Usr, Instruction::BinaryOps::Sub) != nullptr)
     || (canRewriteOp(Usr, Instruction::BinaryOps::FSub) != nullptr)) {
      return true;
    }
  }

  return false;
}

Value* RewriteExpr::getSineArg(Value *I) {
  if (I == nullptr) { return nullptr; }
  CallInst *CI = dyn_cast<CallInst>(I);
  if (CI == nullptr) { return nullptr; }
  if (CI->getNumArgOperands() != 1) { return nullptr; }
  if (MFN->unmangleName(CI->getCalledFunction()->getName()) != "sin") {
    return nullptr;
  }
  return CI->getArgOperand(0);
}

Value* RewriteExpr::getCosineArg(Value *I) {
  if (I == nullptr) { return nullptr; }
  CallInst *CI = dyn_cast<CallInst>(I);
  if (CI == nullptr) { return nullptr; }
  if (CI->getNumArgOperands() != 1) { return nullptr; }
  if (MFN->unmangleName(CI->getCalledFunction()->getName()) != "cos") {
    return nullptr;
  }
  return CI->getArgOperand(0);
}

/// Returns \c true if the instruction is a binary operator with opcode
/// \c TopBinOpcode and which has two operands that are themselfes binary
/// operators with opcode \c LeafBinOpcode. The operands of those two leaf binary
/// operators are stored in the provided pointers. So the function detects binop
/// trees like:
///           TopBinOpcode
///            /        \
///      LeafBin1      LeafBin2
///       /    \        /    \
///     B0O0  B0O1    B1O0  B1O1
/// B0O0, B0O1, B1O0 and B1O1 are returned in the pointers.
bool RewriteExpr::isDoubleBinExpr(Instruction *I, Value **Bin0Op0, Value **Bin0Op1,
    Value **Bin1Op0, Value **Bin1Op1, const unsigned TopBinOpcode,
    const unsigned LeafBinOpcode) {
  if (I == nullptr) { return false; }

  // First the simple cases with all operands being instructions
  if (I->getOpcode() == TopBinOpcode) {
    Value *LeafBinOp0 = I->getOperand(0);
    Value *LeafBinOp1 = I->getOperand(1);
    if ((isa<Instruction>(LeafBinOp0) == false)
     || (isa<Instruction>(LeafBinOp1) == false)) {
      return false;
    }
    Instruction *LeafBinI0 = dyn_cast<Instruction>(LeafBinOp0);
    Instruction *LeafBinI1 = dyn_cast<Instruction>(LeafBinOp1);
    if ((LeafBinI0->getOpcode() != LeafBinOpcode)
     || (LeafBinI1->getOpcode() != LeafBinOpcode)) {
      return false;
    }
    if ((LeafBinI0->getNumOperands() < 2)
     || (LeafBinI1->getNumOperands() < 2)) {
      return false;
    }
    // We have something like ((a*b) + (c*d))
    if (Bin0Op0 != nullptr) { *Bin0Op0 = LeafBinI0->getOperand(0); }
    if (Bin0Op1 != nullptr) { *Bin0Op1 = LeafBinI0->getOperand(1); }
    if (Bin1Op0 != nullptr) { *Bin1Op0 = LeafBinI1->getOperand(0); }
    if (Bin1Op1 != nullptr) { *Bin1Op1 = LeafBinI1->getOperand(1); }
    return true;
  }
  return false;
}

bool RewriteExpr::isMulAddMulExpr(Instruction *I, Value **Mul0Op0, Value **Mul0Op1,
    Value **Mul1Op0, Value **Mul1Op1) {
  if (I == nullptr) { return false; }

  if (isDoubleBinExpr(I, Mul0Op0, Mul0Op1, Mul1Op0, Mul1Op1,
      Instruction::BinaryOps::Add, Instruction::BinaryOps::Mul) == true) {
    return true;
  } else if (isDoubleBinExpr(I, Mul0Op0, Mul0Op1, Mul1Op0, Mul1Op1,
      Instruction::BinaryOps::FAdd, Instruction::BinaryOps::FMul) == true) {
    return true;
  } else if (I->getOpcode() == Instruction::OtherOps::Call) {
    // Now the strange stuff with intrisics with fused multiply-add operations
    CallInst *CI = dyn_cast<CallInst>(I);
    if ((CI->getCalledFunction()->getName() == MFN->mangleName("fmuladd.f32"))
     || (CI->getCalledFunction()->getName() == MFN->mangleName("fmuladd.f64"))) {
      // We have something like
      // %tmp = fmul %c, %d
      // %result = call @llvm.fmuladd(%a, %b, %tmp)
      if (CI->getNumArgOperands() != 3) { return false; }
      Instruction *BinI1 = dyn_cast<Instruction>(CI->getArgOperand(2));
      if (BinI1 == nullptr) { return false; }
      if (BinI1->getOpcode() != Instruction::BinaryOps::FMul) {
        // Seems like only floating pointer args are allowed
        return false;
      }
      if (Mul0Op0 != nullptr) { *Mul0Op0 = CI->getArgOperand(0); }
      if (Mul0Op1 != nullptr) { *Mul0Op1 = CI->getArgOperand(1); }
      if (Mul1Op0 != nullptr) { *Mul1Op0 = BinI1->getOperand(0); }
      if (Mul1Op1 != nullptr) { *Mul1Op1 = BinI1->getOperand(1); }
      return true;
    }
  }

  return false;
}

bool RewriteExpr::isMulSubMulExpr(Instruction *I, Value **Mul0Op0, Value **Mul0Op1,
    Value **Mul1Op0, Value **Mul1Op1) {
  if (I == nullptr) { return false; }

  if (isDoubleBinExpr(I, Mul0Op0, Mul0Op1, Mul1Op0, Mul1Op1,
      Instruction::BinaryOps::Sub, Instruction::BinaryOps::Mul) == true) {
    return true;
  } else if (isDoubleBinExpr(I, Mul0Op0, Mul0Op1, Mul1Op0, Mul1Op1,
      Instruction::BinaryOps::FSub, Instruction::BinaryOps::FMul) == true) {
    return true;
  } else if (I->getOpcode() == Instruction::OtherOps::Call) {
    // Now the strange stuff with intrisics with fused multiply-add operations
    CallInst *CI = dyn_cast<CallInst>(I);
    if ((CI->getCalledFunction()->getName() == MFN->mangleName("fmuladd.f32"))
     || (CI->getCalledFunction()->getName() == MFN->mangleName("fmuladd.f64"))) {
      // We should have something like
      // %tmp = fmul %c, %d
      // %tmp.neg = fsub 0.0, %tmp
      // %result = call @llvm.fmuladd(%a, %b, %tmp.neg)
      if (CI->getNumArgOperands() != 3) { return false; }
      Value *BinOp1 = CI->getArgOperand(2);
      if (BinaryOperator::isFNeg(BinOp1, true) == false) { return false; }
      BinOp1 = BinaryOperator::getFNegArgument(BinOp1);
      Instruction *BinI1 = dyn_cast<Instruction>(BinOp1);
      if (BinI1 == nullptr) { return false; }
      if (BinI1->getOpcode() != Instruction::BinaryOps::FMul) {
        // Seems like only floating pointer args are allowed
        return false;
      }
      if (Mul0Op0 != nullptr) { *Mul0Op0 = CI->getArgOperand(0); }
      if (Mul0Op1 != nullptr) { *Mul0Op1 = CI->getArgOperand(1); }
      if (Mul1Op0 != nullptr) { *Mul1Op0 = BinI1->getOperand(0); }
      if (Mul1Op1 != nullptr) { *Mul1Op1 = BinI1->getOperand(1); }
      return true;
    }
  }
  return false;
}

Value* RewriteExpr::morphMathExpr(Instruction *I) {
  if (I == nullptr) { return nullptr; }

  // The function tries to morph several math patterns (especially sin, cos,...)
  // into easier expressions or constants where possible.

  Value *M0Op0 = nullptr; Value *M0Op1 = nullptr;
  Value *M1Op0 = nullptr; Value *M1Op1 = nullptr;
  if (isMulAddMulExpr(I, &M0Op0, &M0Op1, &M1Op0, &M1Op1) == true) {
    { // Replace: sin^2+cos^2 -> 1.0
      // We have something like ((a*b) + (c*d))
      // So first testing for sin^2 + cos^2
      // Note that this only holds for floating point numbers. As sin and cos are
      // ranged in [0;1] an int value might be 0 in mostly all cases and therefore
      // 0+0 is still 0 (and not 1).
      Value *SinArg0 = nullptr; Value *SinArg1 = nullptr;
      Value *CosArg0 = nullptr; Value *CosArg1 = nullptr;

      SinArg0 = getSineArg(M0Op0);
      if (SinArg0 != nullptr) {
        SinArg1 = getSineArg(M0Op1);
        CosArg0 = getCosineArg(M1Op0);
        CosArg1 = getCosineArg(M1Op1);
      } else {
        SinArg0 = getSineArg(M1Op0);
        SinArg1 = getSineArg(M1Op1);
        CosArg0 = getCosineArg(M0Op0);
        CosArg1 = getCosineArg(M0Op1);
      }

      if ((SinArg0 != nullptr) && (SinArg1 != nullptr)
       && (CosArg0 != nullptr) && (CosArg1 != nullptr)) {
        if ((SinArg0 == SinArg1) && (CosArg0 == CosArg1)
         && (SinArg0 == CosArg0)) { // == should be transitive -> all args are equal
          if (SinArg0->getType()->isFPOrFPVectorTy() == true) {
            DEBUG(dbgs() << "Found sin^2(x)+cos^2(x). Replacing by 1.0\n");
            // Now replace the expression by 1.0
            Constant *ConstFPOne = ConstantFP::get(SinArg0->getType(), 1.0);
            I->replaceAllUsesWith(ConstFPOne);
            Loopus::eraseFromParentRecursively(I, false);
            ++StatsNumMathExprRewritten;
            return ConstFPOne;
          }
        }
      }
    }
    { // Replace: sin(x)cos(y)+cos(x)sin(y) -> sin(x+y)
      Value *Sin0Arg = nullptr; Value *Cos0Arg = nullptr;
      Value *Sin1Arg = nullptr; Value *Cos1Arg = nullptr;
      Value *SineFunc = nullptr;

      Sin0Arg = getSineArg(M0Op0);
      if (Sin0Arg != nullptr) {
        Cos0Arg = getCosineArg(M0Op1);
        SineFunc = dyn_cast<CallInst>(M0Op0)->getCalledFunction();
      } else {
        Sin0Arg = getSineArg(M0Op1);
        Cos0Arg = getCosineArg(M0Op0);
        if (Sin0Arg != nullptr) {
          SineFunc = dyn_cast<CallInst>(M0Op1)->getCalledFunction();
        }
      }
      Sin1Arg = getSineArg(M1Op0);
      if (Sin1Arg != nullptr) {
        Cos1Arg = getCosineArg(M1Op1);
      } else {
        Sin1Arg = getSineArg(M1Op1);
        Cos1Arg = getCosineArg(M1Op0);
      }

      if ((Sin0Arg != nullptr) && (Cos0Arg != nullptr)
       && (Sin1Arg != nullptr) && (Cos1Arg != nullptr)) {
        if ((Sin0Arg == Cos1Arg) && (Cos0Arg == Sin1Arg)) {
          if ((Sin0Arg->getType()->isFPOrFPVectorTy() == true)
           && (Sin0Arg->getType() == Sin1Arg->getType())) {
            DEBUG(dbgs() << "Found sinx*cosy+cosx*siny. Replacing by sin(x+y)\n");
            BinaryOperator *ArgAddI = BinaryOperator::Create(Instruction::BinaryOps::FAdd,
                Sin0Arg, Sin1Arg);
            ArgAddI->setDebugLoc(I->getDebugLoc());
            ArgAddI->insertBefore(I);

            CallInst *SineCI = CallInst::Create(SineFunc, ArgAddI);
            SineCI->setDebugLoc(I->getDebugLoc());
            SineCI->insertAfter(ArgAddI);

            I->replaceAllUsesWith(SineCI);
            Loopus::eraseFromParentRecursively(I, false);
            ++StatsNumMathExprRewritten;
            return SineCI;
          }
        }
      }
    }
    { // Replace: cos(x)*cos(y)+sin(x)*sin(y) -> cos(x-y)
      Value *CosArg0 = nullptr; Value *CosArg1 = nullptr;
      Value *SinArg0 = nullptr; Value *SinArg1 = nullptr;
      Value *CosineFunc = nullptr;

      CosArg0 = getCosineArg(M0Op0);
      if (CosArg0 != nullptr) {
        CosArg1 = getCosineArg(M0Op1);
        SinArg0 = getSineArg(M1Op0);
        SinArg1 = getSineArg(M1Op1);
        CosineFunc = dyn_cast<CallInst>(M0Op0)->getCalledFunction();
      } else {
        CosArg0 = getCosineArg(M1Op0);
        CosArg1 = getCosineArg(M1Op1);
        SinArg0 = getSineArg(M0Op0);
        SinArg1 = getSineArg(M0Op1);
        if (CosArg0 != nullptr) {
          CosineFunc = dyn_cast<CallInst>(M1Op0)->getCalledFunction();
        }
      }

      if ((CosArg0 != nullptr) && (CosArg1 != nullptr)
       && (SinArg0 != nullptr) && (SinArg1 != nullptr)) {
        if (CosArg0 != SinArg0) {
          using std::swap;
          swap(SinArg0, SinArg1);
        }

        if ((CosArg0 == SinArg0) && (CosArg1 == SinArg1)) {
          if ((CosArg0->getType()->isFPOrFPVectorTy() == true)
           && (CosArg0->getType() == CosArg1->getType())) {
            DEBUG(dbgs() << "Found cosx*cosy+sinx*siny. Replacing by cos(x-y)\n");
            BinaryOperator *ArgSubI = BinaryOperator::Create(Instruction::BinaryOps::FSub,
                CosArg0, CosArg1);
            ArgSubI->setDebugLoc(I->getDebugLoc());
            ArgSubI->insertBefore(I);

            CallInst *CosineCI = CallInst::Create(CosineFunc, ArgSubI);
            CosineCI->setDebugLoc(I->getDebugLoc());
            CosineCI->insertAfter(ArgSubI);

            I->replaceAllUsesWith(CosineCI);
            Loopus::eraseFromParentRecursively(I, false);
            ++StatsNumMathExprRewritten;
            return CosineCI;
          }
        }
      }
    }
  } else if (isMulSubMulExpr(I, &M0Op0, &M0Op1, &M1Op0, &M1Op1) == true) {
    { // Replace: sin(x)cos(y)-cos(x)sin(y) -> sin(x-y)
      Value *Sin0Arg = nullptr; Value *Cos0Arg = nullptr;
      Value *Sin1Arg = nullptr; Value *Cos1Arg = nullptr;
      Value *SineFunc = nullptr;

      Sin0Arg = getSineArg(M0Op0);
      if (Sin0Arg != nullptr) {
        Cos0Arg = getCosineArg(M0Op1);
        SineFunc = dyn_cast<CallInst>(M0Op0)->getCalledFunction();
      } else {
        Sin0Arg = getSineArg(M0Op1);
        Cos0Arg = getCosineArg(M0Op0);
        if (Sin0Arg != nullptr) {
          SineFunc = dyn_cast<CallInst>(M0Op1)->getCalledFunction();
        }
      }
      Sin1Arg = getSineArg(M1Op0);
      if (Sin1Arg != nullptr) {
        Cos1Arg = getCosineArg(M1Op1);
      } else {
        Sin1Arg = getSineArg(M1Op1);
        Cos1Arg = getCosineArg(M1Op0);
      }

      if ((Sin0Arg != nullptr) && (Cos0Arg != nullptr)
       && (Sin1Arg != nullptr) && (Cos1Arg != nullptr)) {
        if ((Sin0Arg == Cos1Arg) && (Cos0Arg == Sin1Arg)) {
          if ((Sin0Arg->getType()->isFPOrFPVectorTy() == true)
           && (Sin0Arg->getType() == Sin1Arg->getType())) {
            DEBUG(dbgs() << "Found sinx*cosy-cosx*siny. Replacing by sin(x-y)\n");
            BinaryOperator *ArgSubI = BinaryOperator::Create(Instruction::BinaryOps::FSub,
                Sin0Arg, Sin1Arg);
            ArgSubI->setDebugLoc(I->getDebugLoc());
            ArgSubI->insertBefore(I);

            CallInst *SineCI = CallInst::Create(SineFunc, ArgSubI);
            SineCI->setDebugLoc(I->getDebugLoc());
            SineCI->insertAfter(ArgSubI);

            I->replaceAllUsesWith(SineCI);
            Loopus::eraseFromParentRecursively(I, false);
            ++StatsNumMathExprRewritten;
            return SineCI;
          }
        }
      }
    }
    { // Replace: cos(x)*cos(y)-sin(x)*sin(y) -> cos(x+y)
      Value *CosArg0 = nullptr; Value *CosArg1 = nullptr;
      Value *SinArg0 = nullptr; Value *SinArg1 = nullptr;
      Value *CosineFunc = nullptr;

      CosArg0 = getCosineArg(M0Op0);
      CosArg1 = getCosineArg(M0Op1);
      SinArg0 = getSineArg(M1Op0);
      SinArg1 = getSineArg(M1Op1);

      if ((CosArg0 != nullptr) && (CosArg1 != nullptr)
       && (SinArg0 != nullptr) && (SinArg1 != nullptr)) {
        CosineFunc = dyn_cast<CallInst>(M0Op0)->getCalledFunction();
        if (CosArg0 != SinArg0) {
          using std::swap;
          swap(SinArg0, SinArg1);
        }

        if ((CosArg0 == SinArg0) && (CosArg1 == SinArg1)) {
          if ((CosArg0->getType()->isFPOrFPVectorTy() == true)
           && (CosArg0->getType() == CosArg1->getType())) {
            DEBUG(dbgs() << "Found cosx*cosy-sinx*siny. Replacing by cos(x+y)\n");
            BinaryOperator *ArgAddI = BinaryOperator::Create(Instruction::BinaryOps::FAdd,
                CosArg0, CosArg1);
            ArgAddI->setDebugLoc(I->getDebugLoc());
            ArgAddI->insertBefore(I);

            CallInst *CosineCI = CallInst::Create(CosineFunc, ArgAddI);
            CosineCI->setDebugLoc(I->getDebugLoc());
            CosineCI->insertAfter(ArgAddI);

            I->replaceAllUsesWith(CosineCI);
            Loopus::eraseFromParentRecursively(I, false);
            ++StatsNumMathExprRewritten;
            return CosineCI;
          }
        }
      }
    }
  }

  return nullptr;
}

BinaryOperator* RewriteExpr::morphSubIntoNeg(Instruction *I) {
  if (I == nullptr) { return nullptr; }
  if ((I->getOpcode() != Instruction::BinaryOps::Sub)
   && (I->getOpcode() != Instruction::BinaryOps::FSub)) {
    return nullptr;
  }

  if (I->getOpcode() == Instruction::BinaryOps::Sub) {
    // This sub instruction should be converted into a neg: X-Y -> X+(0-Y)
    DEBUG(dbgs() << "Breaking sub into neg " << *I << "\n");
    BinaryOperator *NewNeg = BinaryOperator::CreateNeg(I->getOperand(1));
    NewNeg->insertBefore(I);
    BinaryOperator *NewAdd = BinaryOperator::Create(Instruction::BinaryOps::Add,
        I->getOperand(0), NewNeg);
    NewAdd->insertAfter(NewNeg);
    NewAdd->setName(I->getName());
    NewAdd->setDebugLoc(I->getDebugLoc());

    I->replaceAllUsesWith(NewAdd);
    Loopus::eraseFromParentRecursively(I);
    return NewAdd;
  } else if (I->getOpcode() == Instruction::BinaryOps::FSub) {
    // This fsub instruction should be converted into a fneg: X-Y -> X+(0-Y)
    DEBUG(dbgs() << "Breaking fsub into fneg " << *I << "\n");
    BinaryOperator *NewNeg = BinaryOperator::CreateFNeg(I->getOperand(1));
    NewNeg->insertBefore(I);
    BinaryOperator *NewAdd = BinaryOperator::Create(Instruction::BinaryOps::FAdd,
        I->getOperand(0), NewNeg);
    NewAdd->insertAfter(NewNeg);
    NewAdd->setName(I->getName());
    NewAdd->setDebugLoc(I->getDebugLoc());

    I->replaceAllUsesWith(NewAdd);
    Loopus::eraseFromParentRecursively(I);
    return NewAdd;
  }

  return nullptr;
}

BinaryOperator* RewriteExpr::morphNegIntoMul(Instruction *I) {
  if (I == nullptr) { return nullptr; }
  if ((BinaryOperator::isNeg(I) == false)
   && (BinaryOperator::isFNeg(I) == false)) {
    return nullptr;
  }

  if (I->getOpcode() == Instruction::BinaryOps::Sub) {
    // This neg should be converted into a mul: (0-X) -> (-1)*X
    DEBUG(dbgs() << "Breaking neg into mul " << *I << "\n");
    Constant *ConstMulOp = ConstantInt::get(I->getOperand(1)->getType(), -1, true);
    BinaryOperator *NewMul = BinaryOperator::Create(Instruction::BinaryOps::Mul,
        ConstMulOp, I->getOperand(1));
    NewMul->insertBefore(I);
    NewMul->setName(I->getName());
    NewMul->setDebugLoc(I->getDebugLoc());

    I->replaceAllUsesWith(NewMul);
    Loopus::eraseFromParentRecursively(I);
    return NewMul;
  } else if (I->getOpcode() == Instruction::BinaryOps::FSub) {
    // The fneg should be converted into a fmul: (0-X) -> (-1)*X. You should
    // know what you are doing as floating points are bad ;-)
    DEBUG(dbgs() << "Breaking fneg into fmul " << *I << "\n");
    Constant *ConstMulOp = ConstantFP::get(I->getOperand(1)->getType(), -1.0);
    BinaryOperator *NewMul = BinaryOperator::Create(Instruction::BinaryOps::FMul,
        ConstMulOp, I->getOperand(1));
    NewMul->insertBefore(I);
    NewMul->setName(I->getName());
    NewMul->setDebugLoc(I->getDebugLoc());

    I->replaceAllUsesWith(NewMul);
    Loopus::eraseFromParentRecursively(I);
    return NewMul;
  }

  return nullptr;
}

bool RewriteExpr::canonicalizeInst(Instruction *I) {
  if (I == nullptr) { return false; }
  if (isa<BinaryOperator>(I) == false) { return false; }

  if (I->isCommutative() == true) {
    if ((isa<Constant>(I->getOperand(0)) == true)
        && (isa<Constant>(I->getOperand(1)) == false)) {
      dyn_cast<BinaryOperator>(I)->swapOperands();
      DEBUG(dbgs() << "Swapped ops for " << *I << "\n");
      return true;
    }
  }
  return false;
}

bool RewriteExpr::handleInst(Instruction *BinOp) {
  if (BinOp == nullptr) { return false; }

  bool Changed = false;
  // We do not handle vector types
  if (BinOp->getType()->isVectorTy() == true) { return Changed; }

  // Check if we want to morph a sub into something different...
  if (BinOp->getOpcode() == Instruction::BinaryOps::Sub) {
    if (canBreakSub(BinOp) == true) {
      BinOp = morphSubIntoNeg(BinOp);
      Changed = true;
    } else if (BinaryOperator::isNeg(BinOp) == true) {
      // If this instructions negates a mul-tree then we want to rewrite the neg
      // into a mul with (-1). Or if the negation is part of a mul tree than we
      // morph it into a mul with (-1).
      if ((canRewriteOp(BinOp->getOperand(1), Instruction::BinaryOps::Mul) != nullptr)
       || ((BinOp->hasOneUse() == true) && (BinOp->user_back()->getOpcode() == Instruction::BinaryOps::Mul))) {
        BinOp = morphNegIntoMul(BinOp);
        Changed = true;
      }
    }
  } else if (BinOp->getOpcode() == Instruction::BinaryOps::FSub) {
    if (canBreakSub(BinOp) == true) {
      BinOp = morphSubIntoNeg(BinOp);
      Changed = true;
    } else if (BinaryOperator::isFNeg(BinOp) == true) {
      // If this instructions negates a mul-tree then we want to rewrite the neg
      // into a mul with (-1). Or if the negation is part of a mul tree than we
      // morph it into a mul with (-1).
      if ((canRewriteOp(BinOp->getOperand(1), Instruction::BinaryOps::FMul) != nullptr)
       || ((BinOp->hasOneUse() == true) && (BinOp->user_back()->getOpcode() == Instruction::BinaryOps::FMul))) {
        BinOp = morphNegIntoMul(BinOp);
        Changed = true;
      }
    }
  }

  if (isa<BinaryOperator>(BinOp) == false) { return Changed; }
  const unsigned BOpCode = BinOp->getOpcode();
  if (BinOp->isCommutative() == false) { return Changed; }
  Changed |= canonicalizeInst(BinOp);
  if (BinOp->isAssociative() == false) { return Changed; }

  // Let's try to skip instructions that will be rewritten later as they are
  // part of bigger expressions
  if (BinOp->hasOneUse() == true) {
    if (BinOp->user_back()->getOpcode() == BOpCode) {
      // This instruction is definitly part of the expression represented by
      // the one user
      return Changed;
    }
  }

  LeafMapTy ExprLeaves;
  Changed |= collectOperands(BinOp, ExprLeaves);
  Changed |= rewriteExpression(ExprLeaves, BinOp);

  return Changed;
}

//===- Implementation of LLVM pass ----------------------------------------===//
INITIALIZE_PASS_BEGIN(RewriteExpr, "loopus-rewexpr", "Rewrite expressions",  false, false)
INITIALIZE_PASS_DEPENDENCY(OpenCLMDKernels)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_END(RewriteExpr, "loopus-rewexpr", "Rewrite expressions",  false, false)

char RewriteExpr::ID = 0;

namespace llvm {
  Pass* createRewriteExprPass() {
    return new RewriteExpr();
  }
}

RewriteExpr::RewriteExpr(void)
 : FunctionPass(ID), DT(nullptr), MFN(nullptr), OK(nullptr) {
}

void RewriteExpr::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<OpenCLMDKernels>();
  AU.addPreserved<OpenCLMDKernels>();
  AU.addRequired<DominatorTreeWrapperPass>();
  AU.addPreserved<DominatorTreeWrapperPass>();
  AU.setPreservesCFG();
}

bool RewriteExpr::runOnFunction(Function &F) {
  DT = &getAnalysis<DominatorTreeWrapperPass>().getDomTree();
  MFN = &Loopus::MangledFunctionNames::getInstance();
  OK = &getAnalysis<OpenCLMDKernels>();

  if ((MFN == nullptr) || (OK == nullptr) || (DT == nullptr)) {
    return false;
  }

  bool Changed = false;
  if (OK->isKernel(&F) == true) {
    for (inst_iterator INSIT = inst_begin(F), INSEND = inst_end(F);
        INSIT != INSEND; ) {
      Instruction *I = &*INSIT;
      ++INSIT;
      morphMathExpr(I);
    }
    for (inst_iterator INSIT = inst_begin(F), INSEND = inst_end(F);
        INSIT != INSEND; ) {
      Instruction *I = &*INSIT;
      ++INSIT;
      Changed |= handleInst(I);
    }
  }

  return Changed;
}
