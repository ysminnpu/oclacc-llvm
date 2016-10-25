//===- PromoteID.h - Replaces OpenCL intrinsic functions ------------------===//
//
// Replaces basic OpenCL built-in functions by an additional parameter for
// the kernel function. Only used built-in functions should get a parameter.
// Currently replaced built-in functions: n.a.
//
//===----------------------------------------------------------------------===//

#ifndef _LOOPUS_PROMOTEID_H_INCLUDE_
#define _LOOPUS_PROMOTEID_H_INCLUDE_

#include "ArgPromotionTracker.h"
#include "LoopusUtils.h"
#include "OpenCLMDKernels.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Utils/ValueMapper.h"

#include <map>
#include <string>

class HDLPromoteID : public llvm::ModulePass {
  private:
    /// \brief For each builtin function the name is mapped to the proper enum
    /// \brief value and a bool indicating if the function takes one argument
    /// \brief or not.
    std::map<std::string, std::pair<BuiltInFunctionCall::BuiltInFunction, bool>> BuiltInNameMap;
    std::map<BuiltInFunctionCall::BuiltInFunction, std::string> BuiltInFunctionNamesMap;

    bool isBuiltInFunctionCall(const llvm::CallInst *CI,
        BuiltInFunctionCall *BIFC) const;

    ArgPromotionTracker *APT;
    OpenCLMDKernels *OCLKernels;

  protected:
    unsigned findUsedBuiltIns(llvm::Function *F,
        llvm::SmallVectorImpl<BuiltInFunctionCall> &UsedBuiltIns) const;
    llvm::Function* createPromotedFunction(const llvm::Function &F,
        llvm::SmallVectorImpl<BuiltInFunctionCall> &UsedBuiltIns);

  public:
    static char ID;

    HDLPromoteID();
    virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
    virtual bool runOnModule(llvm::Module &M) override;
    virtual bool doInitialization(llvm::Module &M) override;
};

#endif

