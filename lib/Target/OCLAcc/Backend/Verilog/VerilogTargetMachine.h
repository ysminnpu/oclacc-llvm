#ifndef VERILOGTARGETMACHINE_H
#define VERILOGTARGETMACHINE_H

#include "../../OCLAccTargetMachine.h"

namespace llvm {

class VerilogTargetMachine : public OCLAccTargetMachine {
  private:
    virtual void anchor();
  public:
    VerilogTargetMachine(const Target &T, StringRef TT,
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

extern Target TheVerilogTarget;

} // end namespace llm

#endif /* VERILOGTARGETMACHINE_H */
