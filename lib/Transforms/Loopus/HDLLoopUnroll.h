//===- HDLLoopUnroll.h - Unroll loopus for HDL generation --- -------------===//
//
// Unroll loopus for generation of HDL code.
//
//===----------------------------------------------------------------------===//

#ifndef _LOOPUS_HDLLOOPUNROLL_H_INCLUDE_
#define _LOOPUS_HDLLOOPUNROLL_H_INCLUDE_

#include "ArgPromotionTracker.h"
#include "HardwareModel.h"
#include "OpenCLMDKernels.h"

#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Metadata.h"
#include "llvm/Pass.h"

#include <string>

class HDLLoopUnroll : public llvm::LoopPass {
private:
  llvm::AssumptionCache *AC;
  ArgPromotionTracker *APT;
  const llvm::DataLayout *DL;
  llvm::LoopInfo *LI;
  OpenCLMDKernels *MDK;
  Loopus::RessourceEstimatorBase *RE;
  llvm::ScalarEvolution *SE;

  unsigned ggT(unsigned a, unsigned b);

protected:
  const llvm::MDNode* getLoopMetadata(const llvm::Loop *L, const std::string &MDName);
  bool hasDisablePragma(llvm::Loop *L);
  unsigned getUnrollPragmaValue(llvm::Loop *L);
  bool hasFullUnrollPragma(llvm::Loop *L);

  unsigned getRequiredWorkGroupSize(const llvm::Function &F, unsigned Dimension);
  const llvm::ConstantInt* getRequiredWorkGroupSizeConst(const llvm::Function &F, unsigned Dimension);
  bool isValidArrayOffset(const llvm::SCEV* ArrayOffset,
      const llvm::SCEV* GIDOp, const llvm::SCEV** Offset, const llvm::SCEV** Scaling);
  bool dependsOnLoop(const llvm::SCEV *SCEVVal, const llvm::Loop *L);
  int computeLoopUnrollCount(const llvm::Loop *L, unsigned *LoopCnt);
  void setLoopUnrolledMD(llvm::Loop *L);
  bool handleLoop(llvm::Loop *L, llvm::LPPassManager &LPM);

  void printSCEVType(const llvm::SCEV* const S) const;


public:
  static char ID;

  HDLLoopUnroll(void);
  ~HDLLoopUnroll(void);
  virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
  virtual bool runOnLoop(llvm::Loop *L, llvm::LPPassManager &LPM) override;

};

#endif
