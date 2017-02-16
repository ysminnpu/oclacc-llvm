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

  super::visit(R);

  return 0;
}

/// \brief Define Kernel in new file.
///
/// TODO: Allow multiple instantiations of the same kernel.
///
int Verilog::visit(Kernel &R) {
  std::string Filename = R.getName()+".v";
  // Local copy of FS since other Blocks create a new global FS for their
  // contents. This avoids passing the FS between functions.
  FileTy FStmp = openFile(Filename);
  FS = FStmp;

  (*FS) << header();

  // Instantiate the Kernel
  KernelModule KM(R);
  (*FS) << KM.declHeader();

  (*FS) << "// Block Wires and Port Muxer\n";
  (*FS) << KM.declBlockWires();

  (*FS) << "// Block Instantiations\n";
  (*FS) << KM.instBlocks();

  (*FS) << KM.instStreams();

  (*FS) << KM.connectWires();

  (*FS) << KM.declFooter();

  //errs() << KM.instStreams();

  super::visit(R);

  FStmp->close();
  return 0;
}

/// \brief Define Block in new file
///
int Verilog::visit(Block &R) {
  std::string Filename = R.getName()+".v";
  // Local copy of FS since other Blocks create a new global FS for their
  // contents. This avoids passing the FS between functions.
  FileTy FStmp = openFile(Filename);
  FS = FStmp;

  (*FS) << header();

  BlockModule BM(R);
  (*FS) << BM.declHeader();

  if (R.isConditional())
    (*FS) << BM.declEnable();

  (*FS) << BM.declFooter();

  super::visit(R);

  FStmp->close();

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

int Verilog::visit(Mux &R) {
  return 0;
}

#undef DEBUG_TYPE
