#include "llvm/Support/raw_ostream.h"

#include "Verilog.h"
#include "FileHeader.h"
#include "VerilogModule.h"

#include "HW/Writeable.h"
#include "HW/typedefs.h"

#include <sstream>


#define DEBUG_TYPE "verilog"

using namespace oclacc;

const std::string conf::to_string(bool B) {
  return B? "true" : "false";
}

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

  (*FS) << KM.declFooter();

  super::visit(R);

  FS->close();

  TheFiles.write(R.getName()+".do");



  // Write flopoco instances
  FileTy FInstF = openFile(R.getName()+".flo");

  (*FInstF) << FInst.str();

  FInstF->close();

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

  // Create instances for all operations
  super::visit(R);

  FS->close();

  return 0;
}

// The following methods create arithmetic cores. The block then instantiates
// them and takes care of the critical path.

/// \brief Generate Stream as BRAM with Addressgenerator
///

int Verilog::visit(Add &R) {
  const std::string BW = std::to_string(R.getBitWidth());
  
  const std::string Arch = std::to_string(conf::IntAdder_Arch);
  const std::string Opt = std::to_string(conf::IntAdder_OptObjective);
  const std::string SRL = std::to_string(conf::IntAdder_SRL);

  FInst << "# " << R.getUniqueName() << "\n";
  FInst << "IntAdder" << " wIn=" << BW;
  FInst << " arch=" << Arch;
  FInst << " optObjective=" << Opt;
  FInst << " SRL=" << SRL;
  FInst << " name=IntAdder_" << BW;
  FInst << "\n";
  return 0;
}

int Verilog::visit(Sub &R) {
  return 0;
}

int Verilog::visit(FAdd &R) {
  assert(R.getIns().size() == 2);

  const std::string isSub = "false";

  const std::string dualPath = conf::to_string(conf::FPAdd_DualPath);

  const std::string WE = std::to_string(R.getExponentBitWidth());
  const std::string WM = std::to_string(R.getMantissaBitWidth());

  FInst << "# " << R.getUniqueName() << "\n";
  FInst << "FPAdd" << " wE=" << WE << " wF=" << WM;
  FInst << " sub=" << isSub;
  FInst << " dualPath=" << dualPath;
  FInst << " name=" << " FPAdd_" << WE << "_" << WM;
  FInst << "\n";
  return 0;
}

int Verilog::visit(FSub &R) {
  return 0;
}
int Verilog::visit(Mul &R) {
  assert(R.getIns().size() == 2);

  const std::string WX = std::to_string(R.getIn(0)->getBitWidth());
  const std::string WY = std::to_string(R.getIn(1)->getBitWidth());
  const std::string WOut = std::to_string(R.getBitWidth());

  bool isSigned = true;

  FInst << "# " << R.getUniqueName() << "\n";
  FInst << "IntMultiplier" << " wX=" << WX << " wY=" << WY << " WOut=" << WOut;
  FInst << " signedIO=" << conf::to_string(isSigned);
  FInst << " name=IntMultiplier_" << WX << "_" << WY << "_" << WOut;
  FInst << "\n";
  return 0;
}
int Verilog::visit(FMul &R) {
  return 0;
}
int Verilog::visit(UDiv &R) {
  return 0;
}
int Verilog::visit(SDiv &R) {
  return 0;
}
int Verilog::visit(FDiv &R) {
  return 0;
}
int Verilog::visit(URem &R) {
  return 0;
}
int Verilog::visit(SRem &R) {
  return 0;
}
int Verilog::visit(FRem &R) {
  return 0;
}
int Verilog::visit(Shl &R) {
  return 0;
}
int Verilog::visit(LShr &R) {
  return 0;
}
int Verilog::visit(AShr &R) {
  return 0;
}
int Verilog::visit(And &R) {
  return 0;
}
int Verilog::visit(Or &R) {
  return 0;
}
int Verilog::visit(Xor &R) {
  return 0;
}


#undef DEBUG_TYPE
