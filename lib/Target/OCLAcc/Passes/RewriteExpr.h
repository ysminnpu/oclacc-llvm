//===- RewriteExpr.h ------------------------------------------------------===//
//
// Rewrites expressions and emits them again in tree order so that they can
// be computed in parallel.
//
//===----------------------------------------------------------------------===//

#ifndef _LOOPUS_REWRITEEXPR_H_INCLUDE_
#define _LOOPUS_REWRITEEXPR_H_INCLUDE_

#include "OCL/NameMangling.h"
#include "OpenCLMDKernels.h"

#include "llvm/ADT/APInt.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Value.h"
#include "llvm/Pass.h"

#include <map>

class RewriteExpr : public llvm::FunctionPass {
  private:
    llvm::DominatorTree *DT;
    const OpenCLMDKernels *OK;

    bool isDoubleBinExpr(llvm::Instruction *I, llvm::Value **Mul1Op1,
        llvm::Value **Mul1Op2, llvm::Value **Mul2Op1, llvm::Value **Mul2Op2,
        const unsigned TopBinOpcode, const unsigned LeafBinOpcode);

  protected:
    typedef std::map<llvm::Value*, llvm::APInt> LeafMapTy;

    llvm::Value* getSineArg(llvm::Value *I);
    llvm::Value* getCosineArg(llvm::Value *I);
    bool isMulAddMulExpr(llvm::Instruction *I, llvm::Value **Mul0Op0,
        llvm::Value **Mul0Op1, llvm::Value **Mul1Op0, llvm::Value **Mul1Op1);
    bool isMulSubMulExpr(llvm::Instruction *I, llvm::Value **Mul0Op0,
        llvm::Value **Mul0Op1, llvm::Value **Mul1Op0, llvm::Value **Mul1Op1);
    llvm::Value* morphMathExpr(llvm::Instruction *I);

    bool canBreakSub(llvm::Instruction *I);
    llvm::BinaryOperator* morphSubIntoNeg(llvm::Instruction *I);
    llvm::BinaryOperator* morphNegIntoMul(llvm::Instruction *I);
    bool canonicalizeInst(llvm::Instruction *I);

    llvm::BinaryOperator* canRewriteOp(llvm::Value *V, const unsigned Opcode,
        const unsigned NumUses = 1);
    bool collectOperands(llvm::Instruction *I, LeafMapTy &Leaves);
    bool rewriteExpression(LeafMapTy &Leaves, llvm::Instruction *I);

    bool handleInst(llvm::Instruction *BinOp);

  public:
    static char ID;

    RewriteExpr(void);
    virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
    virtual bool runOnFunction(llvm::Function &F) override;
};

#endif
