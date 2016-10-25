#ifndef GENERATEDOTPASS_H
#define GENERATEDOTPASS_H

#include "llvm/IR/Instruction.h"

#include "Macros.h"

namespace llvm {

/// \brief Walk through BasicBlocks and add all Values defined in others as
/// output and input.
///
class GenerateDotPass : public ModulePass {
  public:
    static char ID;

  private:

  public:
    GenerateDotPass();
    ~GenerateDotPass();

    NO_COPY_ASSIGN(GenerateDotPass)

    virtual const char *getPassName() const { return "OCLAcc GenerateDotPass"; }
    virtual bool doInitialization(Module &);
    virtual bool doFinalization(Module &);
    virtual void getAnalysisUsage(AnalysisUsage &AU) const;
    virtual bool runOnModule(Module &);
};

GenerateDotPass *createGenerateDotPass();

} //end namespace llvm

#endif /* GENERATEDOTPASS_H */
