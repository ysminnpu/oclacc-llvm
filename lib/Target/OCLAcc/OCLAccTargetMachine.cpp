#include "OCLAccTargetMachine.h"

#include "Backend/Vhdl/VhdlTargetMachine.h"
#include "Backend/Verilog/VerilogTargetMachine.h"
#include "Backend/Dot/DotTargetMachine.h"

#include "llvm/Transforms/Loopus.h"

#include "llvm/PassManager.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"

//InstructionNamerPass
#include "llvm/Transforms/Scalar.h"


using namespace llvm;

extern "C" void LLVMInitializeOCLAccTarget() {
  RegisterTargetMachine<VhdlTargetMachine> X(TheVhdlTarget);
  RegisterTargetMachine<VerilogTargetMachine> Y(TheVerilogTarget);
  RegisterTargetMachine<DotTargetMachine> Z(TheDotTarget);
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

bool OCLAccTargetMachine::addPassesToEmitFile(PassManagerBase &PM,
                                           formatted_raw_ostream &O,
                                           CodeGenFileType FileType,
                                           bool DisableVerify,
                                           AnalysisID StartAfter,
                                           AnalysisID StopAfter) {
  if (FileType != TargetMachine::CGFT_AssemblyFile)
    return true;

  /* Name Instructions to allow mapping of source to generated objects */
  PM.add(createInstructionNamerPass());
  PM.add(createRenameInvalidPass());

  return false;
}
