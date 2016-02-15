#include "OCLAccTargetMachine.h"
#include "OCLAccHWPass.h"
#include "CreateBlocksPass.h"

#include "llvm/PassManager.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"


using namespace llvm;

extern "C" void LLVMInitializeOCLAccTarget() {
  RegisterTargetMachine<OCLAccVhdlTargetMachine> X(TheOCLAccVhdlTarget);
  RegisterTargetMachine<OCLAccVerilogTargetMachine> Y(TheOCLAccVerilogTarget);
}

static std::string computeDataLayout(const OCLAccSubtarget &ST) {
  std::string Ret = "E-m:e";

  // Alignments for 64 bit integers.
  Ret += "-i64:64";

  Ret += "-f128:64-n32";

  Ret += "-S128";

  return Ret;
}

OCLAccTargetMachine::OCLAccTargetMachine(const Target &T, StringRef TT,
    StringRef CPU, StringRef FS, const TargetOptions &Options,
    Reloc::Model RM, CodeModel::Model CM,
    CodeGenOpt::Level OL) :
  TargetMachine(T, TT, CPU, FS, Options),
  Subtarget(TT ,CPU, FS),
  DL(computeDataLayout(Subtarget))
{
  setRequiresStructuredCFG(true);
}

void OCLAccTargetMachine::addAnalysisPasses(PassManagerBase &PM) {
}

bool OCLAccTargetMachine::addPassesToEmitFile(PassManagerBase &PM,
                                           formatted_raw_ostream &O,
                                           CodeGenFileType FileType,
                                           bool DisableVerify,
                                           AnalysisID StartAfter,
                                           AnalysisID StopAfter) {
  if (FileType != TargetMachine::CGFT_AssemblyFile)
    return true;

  PM.add(new OCLAccHWPass(O));

  return false;
}

OCLAccVhdlTargetMachine::OCLAccVhdlTargetMachine(const Target &T, StringRef TT,
    StringRef CPU, StringRef FS, const TargetOptions &Options,
    Reloc::Model RM, CodeModel::Model CM,
    CodeGenOpt::Level OL) :
  OCLAccTargetMachine(T, TT, CPU, FS, Options, RM, CM, OL) { }

  void OCLAccVhdlTargetMachine::anchor() { };

OCLAccVerilogTargetMachine::OCLAccVerilogTargetMachine(const Target &T, StringRef TT,
    StringRef CPU, StringRef FS, const TargetOptions &Options,
    Reloc::Model RM, CodeModel::Model CM,
    CodeGenOpt::Level OL) :
  OCLAccTargetMachine(T, TT, CPU, FS, Options, RM, CM, OL) { }

  void OCLAccVerilogTargetMachine::anchor() { };
