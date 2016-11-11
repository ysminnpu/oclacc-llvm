#ifndef GENERATEDOTPASS_H
#define GENERATEDOTPASS_H

#include "llvm/Pass.h"

#include "Macros.h"

namespace llvm {

/// \brief Walk through BasicBlocks and add all Values defined in others as
/// output and input.
///
class GenerateDot : public ModulePass {
  public:
    static char ID;

  private:

  public:
    GenerateDot();
    ~GenerateDot();

    NO_COPY_ASSIGN(GenerateDot)

    virtual bool doInitialization(Module &);
    virtual bool doFinalization(Module &);
    virtual void getAnalysisUsage(AnalysisUsage &AU) const;
    virtual bool runOnModule(Module &);
};

Pass *createGenerateDotPass();

} //end namespace llvm

#endif /* GENERATEDOTPASS_H */
