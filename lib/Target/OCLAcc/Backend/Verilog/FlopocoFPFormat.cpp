#include "llvm/Support/raw_ostream.h"

#include "FlopocoFPFormat.h"

#include "HW/Arith.h"
#include "HW/Writeable.h"
#include "HW/typedefs.h"
#include "VerilogModule.h"


#define DEBUG_TYPE "flopoco"

using namespace oclacc;

FlopocoFPFormat::FlopocoFPFormat() {
  DEBUG(dbgs() << __PRETTY_FUNCTION__ << "\n");
}

FlopocoFPFormat::~FlopocoFPFormat() {
  DEBUG(dbgs() << __PRETTY_FUNCTION__ << "\n");
}

int FlopocoFPFormat::visit(ScalarPort &R) {
  VISIT_ONCE(R);
  if (R.getParent()->isBlock())
    R.setBitWidth(R.getBitWidth()+2);
  super::visit(R);
  return 0;
}

int FlopocoFPFormat::visit(FPArith &R) {
  VISIT_ONCE(R);
  R.setBitWidth(R.getBitWidth()+2);
  super::visit(R);
  return 0;
}

int FlopocoFPFormat::visit(FPCompare &R) {
  VISIT_ONCE(R);
  R.setBitWidth(R.getBitWidth()+2);
  super::visit(R);
}

#undef DEBUG_TYPE
