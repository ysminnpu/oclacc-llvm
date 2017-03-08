#include "llvm/Support/raw_ostream.h"

#include "Verilog.h"
#include "FileHeader.h"
#include "VerilogModule.h"

#include "HW/Writeable.h"
#include "HW/typedefs.h"

#include <sstream>


#define DEBUG_TYPE "verilog"

using namespace oclacc;

DesignFiles TheFiles;

void DesignFiles::write(const std::string Filename) {
  DEBUG(dbgs() << "Writing " + Filename + "...");

  std::stringstream DoS;
  FileTy DoFile = openFile(Filename);

  DoS << "# top level simulation\n";
  DoS << "# run with 'vsim -do " << Filename << "'\n";
  DoS << "vlib work\n";

  for (const std::string &S : Files) {
    DoS << "vlog " << S << "\n";
  }

  (*DoFile) << DoS.str();
  DoFile->close();

  DEBUG(dbgs() << "done\n");
}

void DesignFiles::addFile(const std::string S) {
  Files.push_back(S);
}


Verilog::Verilog() {
  DEBUG(dbgs() << __PRETTY_FUNCTION__ << "\n");
}

Verilog::~Verilog() {
  DEBUG(dbgs() << __PRETTY_FUNCTION__ << "\n");
}

/// \brief DesignUnits contain multiple different Kernels.
///
/// TODO:
/// Instantiation of Kernels and Blocks works as follows:
/// 1. Visit the kernel, create the KernelModule and set the member pointer to
/// the instance.
/// 2. Visit all Blocks depth-first. Each Block creates a new BlockModule.
///
/// Finally, the Verilog source files are created by querying all subcomponents
/// for their instantiation, e.g. visit(Kernel) calls K->inst() on the
/// KernelModule. Each visit(Block) calls B->inst() on the BlockModules and adds
/// them to the KernelInstance.
/// 
int Verilog::visit(DesignUnit &R) {
  std::string TopFilename = "top.v";
  FileTy FS = openFile(TopFilename);

  TheFiles.addFile(TopFilename);

  for (kernel_p K : R.Kernels) {
    K->accept(*this);
  }

  // no need to call super::visit, all is done here.

  return 0;
}

/// \brief Define Kernel in new file.
///
/// TODO: Allow multiple instantiations of the same kernel.
///
int Verilog::visit(Kernel &R) {
  std::string KernelFilename = R.getName()+".v";
  FileTy FS = openFile(KernelFilename);

  TheFiles.addFile(KernelFilename);

  (*FS) << header();

  // Instantiate the Kernel
  KernelModule KM(R);
  (*FS) << KM.declHeader();

  (*FS) << "// Block wires";
  (*FS) << KM.declBlockWires();

  (*FS) << "// Block instantiations\n";
  (*FS) << KM.instBlocks();

  (*FS) << KM.instStreams();

  (*FS) << KM.connectWires();

  (*FS) << KM.declFooter();

  //errs() << KM.instStreams();

  super::visit(R);

  FS->close();

  TheFiles.write(R.getName()+".do");

  return 0;
}

/// \brief Define Block in new file
///
int Verilog::visit(Block &R) {
  std::string Filename = R.getName()+".v";
  // Local copy of FS since other Blocks create a new global FS for their
  // contents. This avoids passing the FS between functions.
  FileTy FS = openFile(Filename);

  TheFiles.addFile(Filename);

  (*FS) << header();

  BlockModule BM(R);
  (*FS) << BM.declHeader();

  if (R.isConditional())
    (*FS) << BM.declEnable();

  (*FS) << BM.declFooter();

  super::visit(R);

  FS->close();

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
