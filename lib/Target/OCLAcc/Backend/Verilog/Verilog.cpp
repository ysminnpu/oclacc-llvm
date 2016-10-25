#include "llvm/Support/raw_ostream.h"

#include "Verilog.h"
#include "HW/Writeable.h"
#include "HW/typedefs.h"

using namespace oclacc;

Verilog::Verilog() {
  errs() << "Verilog backend created\n";
}

Verilog::~Verilog() {
  errs() << "Verilog backend destroyed\n";
}

int Verilog::visit(DesignUnit &R) {
  return 0;
}
int Verilog::visit(Kernel &R) {
  return 0;
}

int Verilog::visit(Block &R) {
  return 0;
}

int Verilog::visit(ScalarPort &R) {
  return 0;
}


/// \brief Generate Stream as BRAM with Addressgenerator
///
int Verilog::visit(StreamPort &R) {
  return 0;
}

int Verilog::visit(Arith &R) {
  return 0;
}

int Verilog::visit(FPArith &R) {
  return 0;
}
