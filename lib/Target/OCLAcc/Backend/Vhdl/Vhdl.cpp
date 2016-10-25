#include "llvm/Support/raw_ostream.h"

#include "Vhdl.h"
#include "HW/Writeable.h"
#include "HW/typedefs.h"

using namespace oclacc;

Vhdl::Vhdl() {
  errs() << "Vhdl backend created\n";
}

Vhdl::~Vhdl() {
  errs() << "Vhdl backend destroyed\n";
}

int Vhdl::visit(DesignUnit &R) {
  return 0;
}
int Vhdl::visit(Kernel &R) {
  return 0;
}

int Vhdl::visit(Block &R) {
  return 0;
}

int Vhdl::visit(ScalarPort &R) {
  return 0;
}


/// \brief Generate Stream as BRAM with Addressgenerator
///
int Vhdl::visit(StreamPort &R) {
  return 0;
}

int Vhdl::visit(Arith &R) {
  return 0;
}

int Vhdl::visit(FPArith &R) {
  return 0;
}
