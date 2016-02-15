#ifndef OCLACCTARGETMACHINE_H
#define OCLACCTARGETMACHINE_H

#include "OCLAccSubtarget.h"

#include "llvm/IR/DataLayout.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/Support/raw_ostream.h"

namespace llvm {

class formatted_raw_ostream;

/* We do not inherit from LLVMTargetMachine since we do not need any AsmPasses
 * or MachineCode Emitter
 */
class OCLAccTargetMachine : public TargetMachine {
  private:
    OCLAccSubtarget Subtarget;
    const DataLayout DL;

  public:
    OCLAccTargetMachine(const Target &T, StringRef TT,
        StringRef CPU, StringRef FS, const TargetOptions &Options,
        Reloc::Model RM, CodeModel::Model CM,
        CodeGenOpt::Level OL);

    virtual const OCLAccSubtarget *getSubtargetImpl() const { return &Subtarget; }

  virtual void addAnalysisPasses(PassManagerBase &PM);
    virtual bool addPassesToEmitFile(PassManagerBase &PM,
        formatted_raw_ostream &Out,
        CodeGenFileType FileType,
        bool DisableVerify,
        AnalysisID StartAfter,
        AnalysisID StopAfter);

    virtual const DataLayout *getDataLayout() const { return &DL; }

//    virtual TargetPassConfig *createPassConfig(PassManagerBase &PM);
};

class OCLAccVhdlTargetMachine : public OCLAccTargetMachine {
  private:
    virtual void anchor();
  public:
    OCLAccVhdlTargetMachine(const Target &T, StringRef TT,
        StringRef CPU, StringRef FS, const TargetOptions &Options,
        Reloc::Model RM, CodeModel::Model CM,
        CodeGenOpt::Level OL);
};

class OCLAccVerilogTargetMachine : public OCLAccTargetMachine {
  private:
    virtual void anchor();
  public:
    OCLAccVerilogTargetMachine(const Target &T, StringRef TT,
        StringRef CPU, StringRef FS, const TargetOptions &Options,
        Reloc::Model RM, CodeModel::Model CM,
        CodeGenOpt::Level OL);
};

extern Target TheOCLAccVhdlTarget;
extern Target TheOCLAccVerilogTarget;

} // end llvm namespace

#endif /* OCLACCTARGETMACHINE_H */
