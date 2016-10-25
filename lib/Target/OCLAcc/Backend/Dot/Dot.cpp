#include "llvm/Support/raw_ostream.h"

#include "Dot.h"
#include "HW/Writeable.h"
#include "HW/typedefs.h"

using namespace oclacc;

Dot::Dot() {
  errs() << "Dot backend created\n";
}

Dot::~Dot() {
  errs() << "Dot backend destroyed\n";
}

int Dot::visit(DesignUnit &R) {
  return 0;
}
int Dot::visit(Kernel &R) {
  return 0;
}

int Dot::visit(Block &R) {
  return 0;
}

int Dot::visit(ScalarPort &R) {
  return 0;
}


/// \brief Generate Stream as BRAM with Addressgenerator
///
int Dot::visit(StreamPort &R) {
  return 0;
}

int Dot::visit(Arith &R) {
  return 0;
}

int Dot::visit(FPArith &R) {
  return 0;
}
