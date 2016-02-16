#include "OCLAccTargetMachine.h"
#include "OCLAccHWPass.h"
#include "CreateBlocksPass.h"
#include "GenerateVhdlPass.h"
#include "GenerateVerilogPass.h"

#include "llvm/PassManager.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"

static cl::opt<bool> GenerateDesign("oclacc-design", cl::init(true), cl::desc("Output Hardware Design.") );

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

OCLAccTargetMachine::~OCLAccTargetMachine() {}

/// \brief Vhdl Target Machine

OCLAccVhdlTargetMachine::OCLAccVhdlTargetMachine(const Target &T, StringRef TT,
    StringRef CPU, StringRef FS, const TargetOptions &Options,
    Reloc::Model RM, CodeModel::Model CM,
    CodeGenOpt::Level OL) :
  OCLAccTargetMachine(T, TT, CPU, FS, Options, RM, CM, OL) { }

bool OCLAccVhdlTargetMachine::addPassesToEmitFile(PassManagerBase &PM,
                                           formatted_raw_ostream &O,
                                           CodeGenFileType FileType,
                                           bool DisableVerify,
                                           AnalysisID StartAfter,
                                           AnalysisID StopAfter) {
  if (FileType != TargetMachine::CGFT_AssemblyFile)
    return true;

  if (GenerateDesign)
    PM.add(new GenerateVhdlPass());
  else 
    PM.add(new OCLAccHWPass());

  return false;
}

void OCLAccVhdlTargetMachine::anchor() { }

/// \brief Verilog Target Machine

OCLAccVerilogTargetMachine::OCLAccVerilogTargetMachine(const Target &T, StringRef TT,
    StringRef CPU, StringRef FS, const TargetOptions &Options,
    Reloc::Model RM, CodeModel::Model CM,
    CodeGenOpt::Level OL) :
  OCLAccTargetMachine(T, TT, CPU, FS, Options, RM, CM, OL) { }

bool OCLAccVerilogTargetMachine::addPassesToEmitFile(PassManagerBase &PM,
                                           formatted_raw_ostream &O,
                                           CodeGenFileType FileType,
                                           bool DisableVerify,
                                           AnalysisID StartAfter,
                                           AnalysisID StopAfter) {
  if (FileType != TargetMachine::CGFT_AssemblyFile)
    return true;

  if (GenerateDesign)
    PM.add(new GenerateVerilogPass());
  else 
    PM.add(new OCLAccHWPass());

  return false;
}

void OCLAccVerilogTargetMachine::anchor() { }
