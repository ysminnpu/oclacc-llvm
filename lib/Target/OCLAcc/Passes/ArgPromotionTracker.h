//===- ArgPromotionTracker.h ----------------------------------------------===//
//
// Keeps track of all parameters that were promoted from ID function calls.
//
//===----------------------------------------------------------------------===//

#ifndef _LOOPUS_ARGPROMOTIONTRACKER_H_INCLUDE_
#define _LOOPUS_ARGPROMOTIONTRACKER_H_INCLUDE_

#include "LoopusUtils.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

#include <map>
#include <string>

class ArgPromotionTracker : public llvm::ModulePass {
  private:
    struct PromArgEntry {
      llvm::Argument *Argument;
      llvm::Function *Function;
      BuiltInFunctionCall::BuiltInFunction BIF;
      llvm::Constant *CallArg;

      PromArgEntry(void)
       : Argument(nullptr), Function(nullptr),
         BIF(BuiltInFunctionCall::BuiltInFunction::BIF_Undefined), CallArg(nullptr) {
      }
    };

    typedef llvm::SmallVector<struct PromArgEntry, 8> ArgListTy;
    typedef std::map<llvm::Function*, ArgListTy> MapTy;

    MapTy PromotedArgs;
    llvm::IntegerType *MDCallArgType;

  protected:
    std::string resolveBIFToString(const BuiltInFunctionCall::BuiltInFunction BIF) const;
    BuiltInFunctionCall::BuiltInFunction resolveStringToBIF(const std::string &BIFIdent) const;
    llvm::Argument* getArgByIndex(llvm::Function *Func, llvm::Constant *Index) const;
    llvm::Constant* getArgIndex(llvm::Argument *Arg) const;

    // Low-level functions for accessing the metadata list
    llvm::MDNode* findPromotedArgEntry(llvm::Argument *Arg) const;
    bool addPromotedArgEntry(llvm::Argument *Arg,
        BuiltInFunctionCall::BuiltInFunction BIF, llvm::Constant *BIFArgument);
    bool removePromotedArgEntry(llvm::Argument *Arg);
    unsigned removePromotedArgEntriesForFunction(llvm::Function *Func);
    unsigned fillMap(llvm::Module *M);

    // Basic high-level functions
    void addPromotedArgument(llvm::Argument *Arg,
        BuiltInFunctionCall::BuiltInFunction BIF, llvm::Constant *CallArg);
    llvm::Constant* getCallArgForPromotedArgument_Int(const llvm::Argument *Arg);
    llvm::Argument* getPromotedArgument(const llvm::Function *Func,
        const BuiltInFunctionCall::BuiltInFunction BIF, const llvm::Constant *CallArg) const;

  public:
    static char ID;

    ArgPromotionTracker(void);
    virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
    virtual void print(llvm::raw_ostream &O, const llvm::Module *M) const override;
    virtual bool runOnModule(llvm::Module &M) override;

    // High-level functions for querying list of promoted arguments
    bool isPromotedArgument(const llvm::Argument *Arg) const;
    bool hasPromotedArgument(const llvm::Function *Func,
        const BuiltInFunctionCall::BuiltInFunction BIF, const long CallArg) const;
    bool hasPromotedArgument(const llvm::Function *Func,
        const BuiltInFunctionCall::BuiltInFunction BIF) const;
    llvm::Argument* getPromotedArgument(const llvm::Function *Func,
        const BuiltInFunctionCall::BuiltInFunction BIF, const long CallArg) const;
    llvm::Argument* getPromotedArgument(const llvm::Function *Func,
        const BuiltInFunctionCall::BuiltInFunction BIF) const;
    unsigned getAllPromotedArgumentsList(const llvm::Function *Func,
        llvm::SmallVectorImpl<llvm::Argument*> &ArgList);
    unsigned getPromotedArgumentsList(const llvm::Function *Func,
        const BuiltInFunctionCall::BuiltInFunction BIF,
        llvm::SmallVectorImpl<llvm::Argument*> &ArgList);
    bool hasCallArgForPromotedArgument(const llvm::Argument *Arg);
    long getCallArgForPromotedArgument(const llvm::Argument *Arg);
    BuiltInFunctionCall::BuiltInFunction getBIFForPromotedArgument(const llvm::Argument *Arg);

    // High-level functions for modifing list of promoted arguments
    void addPromotedArgument(llvm::Argument *Arg,
        BuiltInFunctionCall::BuiltInFunction BIF, long CallArg);
    void addPromotedArgument(llvm::Argument *Arg,
        BuiltInFunctionCall::BuiltInFunction BIF);
    void forgetPromotedArgument(llvm::Argument *Arg);
    void forgetPromotedFunctionArguments(llvm::Function *Func);

    unsigned size(void) const;
    bool empty(void) const;
};

#endif
