#include "VerilogTargetMachine.h"
#include "GenerateVerilog.h"

#include "llvm/Pass.h"
#include "llvm/PassManager.h"

using namespace llvm;

VerilogTargetMachine::VerilogTargetMachine(const Target &T, StringRef TT,
    StringRef CPU, StringRef FS, const TargetOptions &Options,
    Reloc::Model RM, CodeModel::Model CM,
    CodeGenOpt::Level OL) :
  OCLAccTargetMachine(T, TT, CPU, FS, Options, RM, CM, OL) { }

bool VerilogTargetMachine::addPassesToEmitFile(PassManagerBase &PM,
                                           formatted_raw_ostream &O,
                                           CodeGenFileType FileType,
                                           bool DisableVerify,
                                           AnalysisID StartAfter,
                                           AnalysisID StopAfter) {
  if (FileType != TargetMachine::CGFT_AssemblyFile)
    return true;

  OCLAccTargetMachine::addPassesToEmitFile(PM,O,FileType,DisableVerify,StartAfter,StopAfter);

  PM.add(createGenerateVerilogPass());

  return false;
}

void VerilogTargetMachine::anchor() { }
