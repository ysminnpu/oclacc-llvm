#include "DotTargetMachine.h"
#include "GenerateDot.h"

#include "llvm/PassManager.h"

using namespace llvm;

DotTargetMachine::DotTargetMachine(const Target &T, StringRef TT,
    StringRef CPU, StringRef FS, const TargetOptions &Options,
    Reloc::Model RM, CodeModel::Model CM,
    CodeGenOpt::Level OL) :
  OCLAccTargetMachine(T, TT, CPU, FS, Options, RM, CM, OL) { }

bool DotTargetMachine::addPassesToEmitFile(PassManagerBase &PM,
                                           formatted_raw_ostream &O,
                                           CodeGenFileType FileType,
                                           bool DisableVerify,
                                           AnalysisID StartAfter,
                                           AnalysisID StopAfter) {
  if (FileType != TargetMachine::CGFT_AssemblyFile)
    return true;

  OCLAccTargetMachine::addPassesToEmitFile(PM,O,FileType,DisableVerify,StartAfter,StopAfter);

  PM.add(createGenerateDotPass());

  return false;
}

void DotTargetMachine::anchor() { }
