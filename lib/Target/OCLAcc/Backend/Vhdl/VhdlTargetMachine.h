#ifndef VHDLTARGETMACHINE_H
#define VHDLTARGETMACHINE_H

#include "../../OCLAccTargetMachine.h"

namespace llvm {

class VhdlTargetMachine : public OCLAccTargetMachine {
  private:
    virtual void anchor();
  public:
    VhdlTargetMachine(const Target &T, StringRef TT,
        StringRef CPU, StringRef FS, const TargetOptions &Options,
        Reloc::Model RM, CodeModel::Model CM,
        CodeGenOpt::Level OL);

    virtual bool addPassesToEmitFile(PassManagerBase &PM,
        formatted_raw_ostream &Out,
        CodeGenFileType FileType,
        bool DisableVerify,
        AnalysisID StartAfter,
        AnalysisID StopAfter);
};

extern Target TheVhdlTarget;

} // end namespace llvm

#endif /* VHDLTARGETMACHINE_H */
