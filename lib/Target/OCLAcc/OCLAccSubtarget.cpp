#include "OCLAccSubtarget.h"

#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/raw_ostream.h"

#define GET_SUBTARGETINFO_TARGET_DESC
#define GET_SUBTARGETINFO_CTOR

/* 
 * Normally, they would be defined in a Machine Code Class, which does not exist
 * for OCLAcc
 */

#define DEBUG_TYPE "TargetMachine"
#define GET_SUBTARGETINFO_ENUM
#define GET_SUBTARGETINFO_MC_DESC
#include "OCLAccGenSubtargetInfo.inc"
#undef DEBUG_TYPE

using namespace llvm;

void OCLAccSubtarget::anchor() { }

OCLAccSubtarget::OCLAccSubtarget(
    const std::string &TT,
    const std::string &CPU,
    const std::string &FS) : OCLAccGenSubtargetInfo(TT, CPU, FS) {

  std::string TheCPU = CPU;

  if (TheCPU.empty()) {
    outs() << "No CPU model supplied. Defaulting to s5phq_d5!\n";
    TheCPU="s5phq_d5";
  }
  ParseSubtargetFeatures(CPU, FS);
}
