#include "llvm/Support/raw_ostream.h"

#include "Verilog.h"
#include "FileHeader.h"
#include "VerilogModule.h"

#include "HW/Writeable.h"
#include "HW/typedefs.h"


#define DEBUG_TYPE "verilog"

using namespace oclacc;

Verilog::Verilog() {
  DEBUG(dbgs() << __PRETTY_FUNCTION__ << "\n");;
}

Verilog::~Verilog() {
  DEBUG(dbgs() << __PRETTY_FUNCTION__ << "\n");;
}

int Verilog::visit(DesignUnit &R) {
  std::string Filename = R.getName()+".v";
  FS = openFile(Filename);

  (*FS) << header();

  super::visit(R);

  FS->close();
  return 0;
}
int Verilog::visit(Kernel &R) {
  KernelModule TM(R);
  (*FS) << TM.declHeader();
  (*FS) << TM.declBlockWires();

  (*FS) << "// Wires and Port Muxer between blocks\n";

  (*FS) << "// Block Instantiations\n";
  (*FS) << TM.instBlocks();

  (*FS) << TM.connectWires();

  (*FS) << TM.declFooter();

  super::visit(R);
  return 0;
}

int Verilog::visit(Block &R) {
  VerilogModule M(R);
  (*FS) << M.declHeader();
  (*FS) << M.declFooter();
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

#undef DEBUG_TYPE
