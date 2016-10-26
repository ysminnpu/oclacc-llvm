#ifndef GENERATEVHDLPASS_H
#define GENERATEVHDLPASS_H

#include "llvm/Pass.h"

#include "Macros.h"

namespace llvm {

/// \brief Walk through BasicBlocks and add all Values defined in others as
/// output and input.
///
class GenerateVhdl : public ModulePass {
  public:
    static char ID;

  private:

  public:
    GenerateVhdl();
    ~GenerateVhdl();

    virtual const char *getPassName() const { return "OCLAcc GenerateVhdl"; }
    virtual bool doInitialization(Module &);
    virtual bool doFinalization(Module &);
    virtual void getAnalysisUsage(AnalysisUsage &AU) const;
    virtual bool runOnModule(Module &);
};

Pass *createGenerateVhdlPass();

} //end namespace llvm

#endif /* GENERATEVHDLPASS_H */
