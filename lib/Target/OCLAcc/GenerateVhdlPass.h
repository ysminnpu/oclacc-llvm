#ifndef GENERATEVHDLPASS_H
#define GENERATEVHDLPASS_H

#include "llvm/IR/Instruction.h"

#include "Macros.h"

namespace llvm {

/// \brief Walk through BasicBlocks and add all Values defined in others as
/// output and input.
///
class GenerateVhdlPass : public ModulePass {
  public:
    static char ID;

  private:

  public:
    GenerateVhdlPass();
    ~GenerateVhdlPass();

    NO_COPY_ASSIGN(GenerateVhdlPass)

    virtual const char *getPassName() const { return "OCLAcc GenerateVhdlPass"; }
    virtual bool doInitialization(Module &);
    virtual bool doFinalization(Module &);
    virtual void getAnalysisUsage(AnalysisUsage &AU) const;
    virtual bool runOnModule(Module &);
};

GenerateVhdlPass *createGenerateVhdlPass();

} //end namespace llvm

#endif /* GENERATEVHDLPASS_H */
