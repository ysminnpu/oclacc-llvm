//===- AggregateLoads.h ---------------------------------------------------===//
//
// Aggregate coalescing loads into one single load if possible.
//
//===----------------------------------------------------------------------===//

#ifndef _LOOPUS_AGGREGATELOADS_H_INCLUDE_
#define _LOOPUS_AGGREGATELOADS_H_INCLUDE_

#include "MangledFunctionNames.h"

#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Utils/ValueMapper.h"

#include <map>

class AggregateLoads : public llvm::FunctionPass {
  private:
    bool LimitToBB;
    llvm::AliasAnalysis *AA;
    const llvm::DataLayout *DL;
    llvm::DominatorTree *DT;
    llvm::LoopInfo *LLI;
    const Loopus::MangledFunctionNames *MFN;
    llvm::ScalarEvolution *SE;

  protected:
    typedef std::map<llvm::LoadInst*, llvm::LoadInst*> LoadChainMapTy;
    typedef llvm::SmallVector<llvm::LoadInst*, 8> LoadChainTy;
    typedef llvm::SmallVector<llvm::LoadInst*, 4> LoadChainChunkTy;

    llvm::LoadInst* findMatchingLoad(llvm::LoadInst* const LD, bool &areReversed,
        const llvm::SmallPtrSetImpl<llvm::LoadInst*> *AvoidanceList = nullptr);
    void populateLoadChain(llvm::Function &F, LoadChainMapTy &LoadChain);
    unsigned computeLoadChain(LoadChainMapTy &LoadChain, llvm::LoadInst* const LD,
        llvm::SmallVectorImpl<llvm::LoadInst*> &LDChain);
    unsigned splitIntoLoadChainChunks(llvm::SmallVectorImpl<llvm::LoadInst*> &LDChain,
        llvm::SmallVectorImpl<LoadChainChunkTy> &LDChainChunks);
    bool handleChunk(llvm::SmallVectorImpl<llvm::LoadInst*> &LDChunk,
        llvm::SmallPtrSetImpl<llvm::LoadInst*> &ProcessedLoads);

    llvm::LoadInst* findMatchingLoad(llvm::LoadInst* const LI,
        const bool allowReversed, bool &isReversed);

  public:
    static char ID;

    AggregateLoads(void);
    virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
    virtual bool runOnFunction(llvm::Function &F) override;

    bool handleLoad(llvm::LoadInst *LI);
};

#endif
