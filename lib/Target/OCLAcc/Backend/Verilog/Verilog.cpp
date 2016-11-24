#include "llvm/Support/raw_ostream.h"

#include "Verilog.h"
#include "HW/Writeable.h"
#include "HW/typedefs.h"

#include "Code.h"

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

  super::visit(R);

  FS->close();
  return 0;
}
int Verilog::visit(Kernel &R) {
  (*FS) << moduleDeclHeader(R);

  (*FS) << "// Wires and Port Muxer between blocks\n";
  for (block_p B : R.getBlocks())
    (*FS) << moduleBlockWires(*B);

  (*FS) << "// Block Instantiations\n";
  for (block_p B : R.getBlocks())
    (*FS) << moduleInstBlock(*B);

  for (block_p B : R.getBlocks())
    (*FS) << moduleConnectWires(*B);

  (*FS) << moduleDeclFooter(R);

  super::visit(R);
  return 0;
}

int Verilog::visit(Block &R) {
  (*FS) << moduleDeclHeader(R);
  (*FS) << moduleDeclFooter(R);
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
