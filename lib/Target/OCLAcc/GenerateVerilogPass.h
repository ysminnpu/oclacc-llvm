#ifndef GENERATEVERILOGPASS_H
#define GENERATEVERILOGPASS_H

#include "llvm/IR/Instruction.h"

#include "Macros.h"

namespace llvm {

/// \brief Walk through BasicBlocks and add all Values defined in others as
/// output and input.
///
class GenerateVerilogPass : public ModulePass {
  public:
    static char ID;

  private:

  public:
    GenerateVerilogPass();
    ~GenerateVerilogPass();

    NO_COPY_ASSIGN(GenerateVerilogPass)

    virtual const char *getPassName() const { return "OCLAcc GenerateVerilogPass"; }
    virtual bool doInitialization(Module &);
    virtual bool doFinalization(Module &);
    virtual void getAnalysisUsage(AnalysisUsage &AU) const;
    virtual bool runOnModule(Module &);
};

GenerateVerilogPass *createGenerateVerilogPass();

} //end namespace llvm

#endif /* GENERATEVERILOGPASS_H */
