# include "VhdlTargetMachine.h"
#include "GenerateVhdlPass.h"

#include "llvm/PassManager.h"

using namespace llvm;

VhdlTargetMachine::VhdlTargetMachine(const Target &T, StringRef TT,
    StringRef CPU, StringRef FS, const TargetOptions &Options,
    Reloc::Model RM, CodeModel::Model CM,
    CodeGenOpt::Level OL) :
  OCLAccTargetMachine(T, TT, CPU, FS, Options, RM, CM, OL) { }

bool VhdlTargetMachine::addPassesToEmitFile(PassManagerBase &PM,
                                           formatted_raw_ostream &O,
                                           CodeGenFileType FileType,
                                           bool DisableVerify,
                                           AnalysisID StartAfter,
                                           AnalysisID StopAfter) {
  if (FileType != TargetMachine::CGFT_AssemblyFile)
    return true;

  PM.add(new GenerateVhdlPass());

  return false;
}

void VhdlTargetMachine::anchor() { }
