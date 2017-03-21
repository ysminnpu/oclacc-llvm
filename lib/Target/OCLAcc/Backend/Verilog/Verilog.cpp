#include "llvm/Support/raw_ostream.h"

#include "Verilog.h"
#include "FileHeader.h"
#include "VerilogModule.h"

#include "HW/Writeable.h"
#include "HW/typedefs.h"

#include <cstdio>
#include <sstream>
#include <cstdlib>
#include <set>
#include <unistd.h>
#include <memory>
#include <regex>
#include <map>

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

// Flopoco functions
namespace flopoco {

typedef std::set<std::string> ModuleListTy;

ModuleListTy ModuleList;
std::map<const std::string, unsigned> ModuleLatency;

/// \brief Return latency of module \param M reported by Flopoco
///
///
unsigned getLatency(const std::string M) {
  FileTy Log = openFile(M+"log");

  return 0;
}

/// \brief Generate modules
unsigned generateModules() {
  char *P = std::getenv("FLOPOCO");
  if (!P)
    llvm_unreachable("$FLOPOCO not set");

  std::string Path(P);
 
  if (! sys::fs::can_execute(P))
    llvm_unreachable("Cannot execute $FLOPOCO");

  FileTy Log = openFile("flopoco.log");

  for (const std::string &M : ModuleList) {
    std::stringstream CS;
    CS << Path << " target=" << "Stratix5";
    CS << " frequency=200";
    CS << " plainVHDL=no";
    CS << " " << M;
    CS << " 2>&1";

    DEBUG(dbgs() << "[exec] " << CS.str() << "\n");

    (*Log) << "[exec] " << CS.str() << "\n";

    std::array<char, 128> buffer;
    std::string Result;
    FILE* Pipe = popen(CS.str().c_str(), "r");

    if (!Pipe)
      llvm_unreachable("popen() $FLOPOCO failed");

    while (!feof(Pipe)) {
      if (fgets(buffer.data(), 128, Pipe) != NULL)
        Result += buffer.data();
    }

    
    if (int R = pclose(Pipe))
      llvm_unreachable("Returned nonzero");

    (*Log) << Result;
    (*Log) << "\n";
    Log->flush();

    // Extract Name from Command; Pattern: name=<name>
    std::regex RgxName("(?:name=)\\S+(?= )");
    std::smatch NameMatch;
    if (!std::regex_search(M, NameMatch, RgxName))
      llvm_unreachable("Getting name failed");

    std::string Name = NameMatch[0];
    Name = Name.substr(Name.find("=")+1);

    // Extract FileName from Command; Pattern: outputFile=<name>
    std::regex RgxFileName("(?:outputFile=)\\S+(?= )");
    std::smatch FileNameMatch;
    if (!std::regex_search(M, FileNameMatch, RgxFileName))
      llvm_unreachable("Getting outputFile failed");

    std::string FileName = FileNameMatch[0];
    FileName = FileName.substr(FileName.find("=")+1);

    // Look for pipeline depth; Pattern: Entity: <name>
    std::regex RgxNoPipe("(?:\\n\\s+Not pipelined)(?=\\n)");
    std::regex RgxPipe("(?:\\n\\s+Pipeline depth = )\\d+(?=\\n)");

    std::smatch Match;

    unsigned Latency = 0;

    if (std::regex_search(Result, Match, RgxNoPipe)) {
      // pass
    } else if (std::regex_search(Result, Match, RgxPipe)) {
      std::smatch NumMatch;
      std::string Res = Match[0];
      std::regex_search(Res, NumMatch, std::regex("\\d+"));

      Latency = stoul(NumMatch[0]);

    } else
      llvm_unreachable("Invalid flopoco output");

    ModuleLatency[Name] = Latency;

    TheFiles.addFile(FileName);

    DEBUG(dbgs() << "Latency of " << Name << ": " << Latency << " clock cycles\n");
  }

  Log->close();

  return 0;
}

const std::string printModules() {
  std::stringstream SS;
  for (const std::string &S : ModuleList) {
    SS << S << "\n";
  }

  return SS.str();
}

/// \brief Add module to global module list
void addModule(const std::string M) {
  ModuleList.insert(M);
}

} // end ns flopoco


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

  FS->close();

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

  // Dump and Generate Flopoco instances
  DEBUG(dbgs() << Line << "\n");
  DEBUG(dbgs() << "[flopoco_instances]\n");
  DEBUG(dbgs() << flopoco::printModules());
  DEBUG(dbgs() << Line << "\n");

  flopoco::generateModules();

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

  const std::string Name = "IntAdder_" + BW;

  std::stringstream FInst;

  FInst << "IntAdder" << " wIn=" << BW << " ";
  FInst << "arch=" << Arch << " ";
  FInst << "optObjective=" << Opt << " ";
  FInst << "SRL=" << SRL << " ";
  FInst << "name=" << Name << " ";
  FInst << "outputFile=" << Name << ".vhd" << " ";

  flopoco::addModule(FInst.str());

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

  const std::string Name = "FPAdd_" + WE + "_" + WM;

  std::stringstream FInst;

  FInst << "FPAdd" << " wE=" << WE << " wF=" << WM << " ";
  FInst << "sub=" << isSub << " ";
  FInst << "dualPath=" << dualPath << " ";
  FInst << "name=" << Name << " ";
  FInst << "outputFile=" << Name << ".vhd" << " ";

  flopoco::addModule(FInst.str());

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

  const std::string Name = "IntMultiplier_" + WX + "_" + WY + "_" + WOut;

  std::stringstream FInst;

  FInst << "IntMultiplier" << " wX=" << WX << " wY=" << WY << " WOut=" << WOut << " ";
  FInst << "signedIO=" << conf::to_string(isSigned) << " ";
  FInst << "name=" << Name << " ";
  FInst << "outputFile=" << Name << ".vhd" << " ";

  flopoco::addModule(FInst.str());

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
