//===- FindAllPaths.h - Find all paths between two BasicBlocks  -----------===//
//
// 
// 
//
//
//===----------------------------------------------------------------------===//

#ifndef FINDALLPATHS_H
#define FINDALLPATHS_H

#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Utils/ValueMapper.h"

#include <vector>
#include <queue>
#include <list>

class FindAllPaths : public llvm::FunctionPass {
  public:
    typedef std::vector<const llvm::BasicBlock *> SinglePathTy;
    typedef SinglePathTy::iterator SinglePathIt;
    typedef SinglePathTy::const_iterator SinglePathConstIt;
    typedef SinglePathTy::const_reverse_iterator SinglePathConstReverseIt;

    typedef std::vector<SinglePathTy> PathTy;

  private:
    PathTy Paths;

    void walkPaths(const llvm::BasicBlock *);

  protected:

  public:
    static char ID;

    FindAllPaths();

    const PathTy getPathForValue(const llvm::BasicBlock *F, const llvm::BasicBlock *T, const llvm::Value *) const;
    const PathTy getPathFromTo(const llvm::BasicBlock *F, const llvm::BasicBlock *T) const;

    virtual void getAnalysisUsage(llvm::AnalysisUsage &) const override;
    virtual bool runOnFunction(llvm::Function &) override;
    virtual bool doInitialization(llvm::Module &) override;
};

namespace FindPaths {

void dump(FindAllPaths::SinglePathTy);
void dump(FindAllPaths::PathTy);

} // end ns FindPaths


#endif /* FINDALLPATHS_H */

