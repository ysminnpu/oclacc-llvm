//===- BitWidthAnalysis.h - Analysis bitwidth of instructions -------------===//
//
// Determines the bitwidth of the result for each instruction.
//
//===----------------------------------------------------------------------===//

#ifndef _LOOPUS_BITWIDTHANALYSIS_H_INCLUDE_
#define _LOOPUS_BITWIDTHANALYSIS_H_INCLUDE_

#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

#include <map>

namespace Loopus {
  enum ExtKind {
    Undef     = 0,      // Unknown/undefined extension
    SExt,               // Extend using sign bit
    ZExt,               // Extend by prepending zeros
    OneExt,             // Extend by prepending ones
    FPNoExt             // FP number => no extension needed
  };
}

class BitWidthAnalysis : public llvm::FunctionPass {
  private:
    llvm::ScalarEvolution *SE;
    const llvm::DataLayout *DL;

    /// The struct is used to represent the bitwidth for a certain value.
    /// Therefor it stores the actual bitwidth provided by the value. Next
    /// the bitwidth required by the users are noted. It could be possible that
    /// the users require a higher bitwidth than the value is able to provide.
    /// The \c MaxBitwidth is not used for computation but stores the bitwidth
    /// that the instruction is able to provide at most (must not be up-to-date).
    /// If the value range could be determined by SCEV or for constants the
    /// needed bitwidth is stored and will be used for internal masking.
    struct BitWidth {
      // The bitwidth that is indicated by the type of the value in the source.
      // Once set this value is not normally changed anymore.
      int TypeWidth;
      // The current bitwidth of the provided result.
      int OutBitwidth;
      // The required bitwidth by the users of the instruction.
      int RequiredBitwidth;
      int PrevIterRequiredBitwidth;
      // The bitwidth that is at most possible (not used for any computations).
      int MaxBitwidth;
      // The bitwidth of a possibly existing internal value mask. The bitwidth
      // is determined on the value range computed by SCEV.
      int ValueMaskBitwidth;
      // Indicates if the Result has to be sign extended (or zero extended).
      Loopus::ExtKind Ext;
      // Indicates if the information are valid.
      bool Valid;

      BitWidth(void)
        : TypeWidth(-1), OutBitwidth(-1), RequiredBitwidth(-1),
        PrevIterRequiredBitwidth(-1), MaxBitwidth(-1), ValueMaskBitwidth(-1),
        Ext(Loopus::ExtKind::Undef), Valid(false) {
      }
    };

    typedef std::map<const llvm::Value*, struct BitWidth> BitWidthMapTy;
    typedef std::map<std::pair<const llvm::Constant*, const llvm::Instruction*>,
            struct BitWidth> ConstantBitWidthMapTy;
    // Stores the bitwidth information for any non-constant values
    BitWidthMapTy BWMap;
    // Stores the bitwidth information for constants used in a certain instruction
    ConstantBitWidthMapTy ConstantBWMap;

  protected:
    int getBitWidthConstant(const llvm::Constant *C, bool isSigned,
        const llvm::Instruction *OwningI);
    std::pair<int, Loopus::ExtKind> getBitWidthRaw(const llvm::Value *V,
        bool isSigned, const llvm::Instruction *OwningI);
    int getBitWidth(const llvm::Value *V, bool isSigned,
        const llvm::Instruction *OwningI);
    int getBitWidthLargestOp(const llvm::Instruction *I, bool isSigned);

    // Forward transition functions
    bool forwardUpdateWidth(struct BitWidth &BWInfo,
        int newOutWidth, int newValueMaskWidth, int newMaxWidth);
    bool forwardUpdateOrInsertWidth(const llvm::Instruction *I,
        int newOutWidth, int newValueMaskWidth, int newMaxWidth, int newTypeWidth,
        Loopus::ExtKind newExt);
    bool forwardSetWidth(struct BitWidth &BWInfo,
        int newOutWidth, int newValueMaskWidth, int newMaxWidth);
    bool forwardSetOrInsertWidth(const llvm::Instruction *I,
        int newOutWidth, int newValueMaskWidth, int newMaxWidth, int newTypeWidth,
        Loopus::ExtKind newExt);
    bool forwardHandleAddSub(const llvm::BinaryOperator *AI);
    bool forwardHandleMul(const llvm::BinaryOperator *MI);
    bool forwardHandleSDiv(const llvm::BinaryOperator *SDI);
    bool forwardHandleUDiv(const llvm::BinaryOperator *UDI);
    bool forwardHandleSRem(const llvm::BinaryOperator *SRI);
    bool forwardHandleURem(const llvm::BinaryOperator *URI);
    bool forwardHandleAnd(const llvm::BinaryOperator *AI);
    bool forwardHandleOr(const llvm::BinaryOperator *OI);
    bool forwardHandleShl(const llvm::BinaryOperator *SI);
    bool forwardHandleShr(const llvm::BinaryOperator *SI,
        Loopus::ExtKind LeadingExt);
    bool forwardHandleTrunc(const llvm::TruncInst *TI);
    bool forwardHandleExt(const llvm::CastInst *EI,
        Loopus::ExtKind LeadingExt);
    bool forwardHandleFPToI(const llvm::CastInst *CI,
        Loopus::ExtKind LeadingExt);
    bool forwardHandlePtrInt(const llvm::CastInst *PII);
    bool forwardHandleBitcast(const llvm::CastInst *BI);
    bool forwardHandleAlloca(const llvm::AllocaInst *AI);
    bool forwardHandleLoad(const llvm::LoadInst *LI);
    bool forwardHandleGEP(const llvm::GetElementPtrInst *GEPI);
    bool forwardHandleCmp(const llvm::CmpInst *CI);
    bool forwardHandleSelect(const llvm::Instruction *SI);
    bool forwardHandleCall(const llvm::CallInst *CI);
    bool forwardHandleFP(const llvm::Instruction *FPI);
    bool forwardHandleDefault(const llvm::Instruction *I);
    bool forwardPropagateBlock(const llvm::BasicBlock *BB);
    bool forwardPropagate(llvm::Function &F);

    bool backwardSetRequest(const llvm::Value *V, int newReqWidth);
    int backwardHandleReqOrMaxop(const llvm::Instruction *AI);
    int backwardHandleAddSub(const llvm::BinaryOperator *AI);
    int backwardHandleAnd(const llvm::BinaryOperator *AI);
    int backwardHandleShl(const llvm::BinaryOperator *SI);
    int backwardHandleShr(const llvm::BinaryOperator *SI);
    int backwardHandleTrunc(const llvm::TruncInst *TI);
    int backwardHandleGeneric(const llvm::Instruction *I);
    bool backwardPropagateBlock(const llvm::BasicBlock *BB);
    bool backwardPropagate(llvm::Function &F);

    void printDBG(const llvm::Instruction *I);
    void printAllDBG(const llvm::Function &F);

  public:
    std::pair<int, Loopus::ExtKind> getBitWidth(const llvm::Value *V,
        const llvm::Instruction *OwningI = 0);

  public:
    static char ID;

    BitWidthAnalysis(void);
    void printBitWidth(llvm::raw_ostream &O, const struct BitWidth &BW) const;
    virtual void print(llvm::raw_ostream &O, const llvm::Module *M) const override;
    virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
    virtual bool runOnFunction(llvm::Function &F) override;
};

#endif

