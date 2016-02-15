#ifndef OCLACCHWPASS_H
#define OCLACCHWPASS_H

#include "llvm/IR/Argument.h"
#include "llvm/Pass.h"

#include "OCLAccTargetMachine.h"
#include "HW/typedefs.h"


namespace llvm {

class formatted_raw_ostream;
class AnalysisUsage;

class OCLAccHWVisitor;

class OCLAccHWPass : public ModulePass {

  private:
    raw_ostream &Log;
    void createMakefile();

    void handleArgument(const Argument &);

  public:
    OCLAccHWPass(raw_ostream &);
    ~OCLAccHWPass();

    OCLAccHWPass (const OCLAccHWPass &) = delete;
    OCLAccHWPass &operator =(const OCLAccHWPass &) = delete;

    virtual const char *getPassName() const { return "OCLAcc OCLAccHWPass"; }

    virtual bool doInitialization(Module &);
    virtual bool doFinalization(Module &);

    virtual bool runOnModule(Module &);

    virtual void getAnalysisUsage(AnalysisUsage &AU) const;

    typedef std::map<const Value *, oclacc::base_p> ValueMapType;
    typedef ValueMapType::iterator ValueMapIt;

    static char ID;

  private:
    ValueMapType ValueMap;
    oclacc::kernel_p HWKernel;
};

} //end namespace llvm

#endif /* OCLACCHWPASS_H */
