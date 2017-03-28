#ifndef GENERATEVERILOGPASS_H
#define GENERATEVERILOGPASS_H

#include "llvm/Pass.h"

namespace llvm {

/// \brief Walk through BasicBlocks and add all Values defined in others as
/// output and input.
///
class GenerateVerilog : public ModulePass {
  public:
    static char ID;

  private:

  public:
    GenerateVerilog();
    ~GenerateVerilog();

    virtual bool doInitialization(Module &);
    virtual bool doFinalization(Module &);
    virtual void getAnalysisUsage(AnalysisUsage &AU) const;
    virtual bool runOnModule(Module &);
};

Pass *createGenerateVerilogPass();

} //end namespace llvm

#endif /* GENERATEVERILOGPASS_H */
