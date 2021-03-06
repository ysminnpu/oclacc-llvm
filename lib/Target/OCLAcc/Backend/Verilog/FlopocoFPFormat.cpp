#include "llvm/Support/raw_ostream.h"

#include "FlopocoFPFormat.h"

#include "HW/Arith.h"
#include "HW/Writeable.h"
#include "HW/typedefs.h"
#include "VerilogModule.h"
#include "Macros.h"


#define DEBUG_TYPE "flopocofp"

using namespace oclacc;

FlopocoFPFormat::FlopocoFPFormat() {
  ODEBUG(__PRETTY_FUNCTION__);
}

FlopocoFPFormat::~FlopocoFPFormat() {
  ODEBUG(__PRETTY_FUNCTION__);
}

int FlopocoFPFormat::visit(ConstVal &R) {
  Datatype T = R.getDatatype();

  if (T == Half || T == Float || T == Double) {
    R.setBits("00" + R.getBits());
    R.setBitWidth(R.getBitWidth()+2);
    ODEBUG("set bitwidth for constant " << R.getUniqueName());
  }
  super::visit(R);
  return 0;
}

int FlopocoFPFormat::visit(ScalarPort &R) {
  VISIT_ONCE(R);
  if (R.isFP()) {
    R.setBitWidth(R.getBitWidth()+2);
    ODEBUG("set bitwidth for " << R.getUniqueName());
  }
  super::visit(R);
  return 0;
}

int FlopocoFPFormat::visit(StreamPort &R) {
  VISIT_ONCE(R);
  if (R.isFP()) {
    R.setBitWidth(R.getBitWidth()+2);
    ODEBUG("set bitwidth for " << R.getUniqueName());
  }
  super::visit(R);
  return 0;
}

int FlopocoFPFormat::visit(StreamAccess &R) {
  VISIT_ONCE(R);

  streamport_p S = R.getStream();

  if (S->isFP()) {
    R.setBitWidth(R.getBitWidth()+2);
    ODEBUG("set bitwidth for " << R.getUniqueName());
  }
  super::visit(R);
  return 0;
}

int FlopocoFPFormat::visit(FPArith &R) {
  VISIT_ONCE(R);
  R.setBitWidth(R.getBitWidth()+2);
  ODEBUG("set bitwidth for " << R.getUniqueName());
  super::visit(R);
  return 0;
}

int FlopocoFPFormat::visit(FPCompare &R) {
  VISIT_ONCE(R);
  R.setBitWidth(R.getBitWidth()+2);
  ODEBUG("set bitwidth for " << R.getUniqueName());
  super::visit(R);
}

#ifdef DEBUG_TYPE
#undef DEBUG_TYPE
#endif
