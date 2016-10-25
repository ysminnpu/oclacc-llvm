#ifndef DOTTARGETMACHINE_H
#define DOTTARGETMACHINE_H

#include "../../OCLAccTargetMachine.h"

namespace llvm {

class DotTargetMachine : public OCLAccTargetMachine {
  private:
    virtual void anchor();
  public:
    DotTargetMachine(const Target &T, StringRef TT,
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

extern Target TheDotTarget;

} // end namespace llm

#endif /* DOTTARGETMACHINE_H */
