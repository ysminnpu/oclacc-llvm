//===- ShiftRegisterDetection.h - Detect shift register loops -------------===//
//
// Detect loops that could be transformed into shiftregisters and set a
// llvm.loop.unroll.disable metadata for those loops.
//
//===----------------------------------------------------------------------===//

#ifndef _LOOPUS_SHIFTREGISTERDETECTION_H_INCLUDE_
#define _LOOPUS_SHIFTREGISTERDETECTION_H_INCLUDE_

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/Pass.h"

#include <string>

class ShiftRegisterDetection : public llvm::FunctionPass {
private:
  enum LoopResult {NoSR=0, PartialSR, FullSR};
  enum ShiftDirection {Undef=0, ArrShlInc, ArrShrDec};

  struct ShiftRegister {
    llvm::Loop *Loop;
    const llvm::Value *BaseArray;
    const llvm::PHINode *LoopCounter;
    const llvm::LoadInst *RegLoad;
    const llvm::StoreInst *RegStore;
    const llvm::BranchInst *LoopBranch;
    const llvm::Value *LoopCond;
    const llvm::ConstantInt *ShiftLength;
    LoopResult Result;
    ShiftDirection ShDir;

    ShiftRegister()
     : Loop(0), BaseArray(0), LoopCounter(0), RegLoad(0), RegStore(0),
       LoopBranch(0), LoopCond(0), ShiftLength(0), Result(LoopResult::NoSR),
       ShDir(ShiftDirection::Undef) {
    }
    ShiftRegister(llvm::Loop *L)
     : Loop(L), BaseArray(0), LoopCounter(0), RegLoad(0), RegStore(0),
       LoopBranch(0), LoopCond(0), ShiftLength(0), Result(LoopResult::NoSR),
       ShDir(ShiftDirection::Undef) {
    }
  };

  bool isAddOne(const llvm::Instruction *I, const llvm::Value *Op) const;
  bool isSubOne(const llvm::Instruction *I, const llvm::Value *Op) const;
  const llvm::Constant* getPositiveOpOfAdd(const llvm::BinaryOperator *Add,
      const llvm::Value *Op) const;
  const llvm::Constant* getPositiveOpOfSub(const llvm::BinaryOperator *Sub,
      const llvm::Value *Op) const;
  bool isAllowedType(const llvm::Type *Ty) const;

  llvm::MDNode* findLoopMetadata(llvm::Loop *L, const std::string &MDName) const;
  bool hasLoopUnrollEnableMetadata(llvm::Loop *L) const;
  void addLoopStringMetadata(llvm::Loop *L, const std::string &MDContents) const;
  void removeLoopStringMetadata(llvm::Loop *L, const std::string &MDContents) const;
  void writeFullSRLoopMetadata(llvm::Loop *L) const;
  void writePartialSRLoopMetadata(llvm::Loop *L) const;
  bool replaceShiftLoop(ShiftRegister &SR);

  bool checkLoopWrapper(llvm::Loop *L, ShiftRegister *SROut = 0);
  bool checkLoop(llvm::Loop *L, const llvm::StoreInst *StInst,
      const llvm::LoadInst *LdInst, ShiftRegister *SROut = 0);

protected:
  bool isSafeBaseArray(const llvm::Value *V) const;
  bool handleLoop(llvm::Loop *L);

public:
  static char ID;

  ShiftRegisterDetection();
  virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
  virtual bool runOnFunction(llvm::Function &F) override;
};

#endif

