#include "llvm/Support/raw_ostream.h"

#include "Verilog.h"
#include "FileHeader.h"
#include "VerilogModule.h"
#include "Naming.h"
#include "VerilogMacros.h"

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

OperatorInstances TheOps;

// Local Port functions

Signal Clk("clk", 1, Signal::In, Signal::Wire);
Signal Rst("rst", 1, Signal::In, Signal::Wire);


void DesignFiles::write(const std::string Filename) {
  DEBUG(dbgs() << "Writing " + Filename + "...");

  std::stringstream DoS;
  FileTy DoFile = openFile(Filename);

  DoS << "# top level simulation\n";
  DoS << "# run with 'vsim -do " << Filename << "'\n";
  DoS << "vlib work\n";

  for (const std::string &S : Files) {
    if (ends_with(S, ".vhd")) {
      DoS << "vcom " << S << "\n";
    } else if (ends_with(S, ".v")) {
      DoS << "vlog " << S << "\n";
    } else
      llvm_unreachable("Invalid filetype");
  }

  (*DoFile) << DoS.str();
  (*DoFile) << "quit\n";
  DoFile->close();

  DEBUG(dbgs() << "done\n");
}

void DesignFiles::addFile(const std::string S) {
  Files.push_back(S);
}

bool OperatorInstances::existsOperator(const std::string OpName) {
  return (Ops.find(OpName) != Ops.end());
}

void OperatorInstances::addOperator(const std::string HWName, const std::string OpName, unsigned Cycles) {
  op_p O;

  if (existsOperator(OpName)) {
    O = getOperator(OpName);
  } else {
    O = std::make_shared<Operator>(OpName, Cycles);
    NameMap[OpName] = O;
    Ops.insert(OpName);
  }

  HWOp[HWName] = O;

}

op_p OperatorInstances::getOperator(const std::string OpName) {
  if (!existsOperator(OpName))
    llvm_unreachable("No Op");

  OpMapConstItTy OI = NameMap.find(OpName);
  if (OI == NameMap.end())
    llvm_unreachable("No Namemapping");

  return OI->second;
}

bool OperatorInstances::existsOperatorForHW(const std::string HWName) {
  return HWOp.find(HWName) != HWOp.end();
}

op_p OperatorInstances::getOperatorForHW(const std::string HWName) {
  assert(existsOperatorForHW(HWName));

  OpMapConstItTy OI = HWOp.find(HWName);

  return OI->second;
}


// Flopoco functions
namespace flopoco {

typedef std::map<std::string, std::string> ModMapTy;
typedef ModMapTy::const_iterator ModMapConstItTy;
typedef std::pair<std::string, std::string> ModMapElem;

// Map Name to ModuleInstantiation
ModMapTy ModuleMap;
// Map HW.getUniqueName to Modulename
ModMapTy NameHWMap;

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

  for (const ModMapElem &M : ModuleMap) {

    // Look up HWName to create Operator
    const std::string Name = M.first;

    ModMapConstItTy NM = NameHWMap.find(Name);
    if (NM == NameHWMap.end())
      llvm_unreachable("No HW Name mapping");

    const std::string HWName = NM->second;
    

    std::stringstream CS;
    CS << Path << " target=" << "Stratix5";
    CS << " frequency=200";
    CS << " plainVHDL=no";
    CS << " " << M.second;
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


    // Extract FileName from Command; Pattern: outputFile=<name>
    std::regex RgxFileName("(?:outputFile=)\\S+(?= )");
    std::smatch FileNameMatch;
    if (!std::regex_search(M.second, FileNameMatch, RgxFileName))
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

    TheOps.addOperator(HWName, Name, Latency);

    TheFiles.addFile(FileName);

    DEBUG(dbgs() << "Latency of " << Name << ": " << Latency << " clock cycles\n");
  }

  Log->close();

  return 0;
}

const std::string printModules() {
  std::stringstream SS;
  for (const ModMapElem &S : ModuleMap) {
    SS << S.first << ":" << S.second << "\n";
  }

  return SS.str();
}

/// \brief Add module to global module list
void addModule(const std::string HWName, const std::string Name, const std::string M) {
  ModuleMap[Name] = M;
  NameHWMap[Name] = HWName;
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
  VISIT_ONCE(R);
  std::string TopFilename = "top.v";
  FileTy FS = openFile(TopFilename);

  TheFiles.addFile(TopFilename);

  for (kernel_p K : R.Kernels) {
    K->accept(*this);
  }

  // no need to call super::visit, all is done here.

  FS->close();

  TheFiles.write(R.getName()+".do");

  return 0;
}

/// \brief Define Kernel in new file.
///
/// TODO: Allow multiple instantiations of the same kernel.
///
int Verilog::visit(Kernel &R) {
  VISIT_ONCE(R);
  std::string KernelFilename = R.getName()+".v";
  FileTy FS = openFile(KernelFilename);

  TheFiles.addFile(KernelFilename);

  (*FS) << header();

  // Instantiate the Kernel
  KernelModule KM(R);
  (*FS) << KM.declHeader();

  (*FS) << "// Block wires\n";
  (*FS) << KM.declBlockWires();

  (*FS) << "// Block instantiations\n";
  (*FS) << KM.instBlocks();

  (*FS) << KM.declFooter();

  super::visit(R);

  FS->close();


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
  VISIT_ONCE(R);
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


  // Clean up Components
  ConstSignals.str("");
  ConstSignals << "// Constant signals\n";

  BlockSignals.str("");
  BlockSignals << "// Component signals\n";

  BlockComponents.str("");
  BlockComponents << "// Component instances\n";

  // Create instances for all operations
  super::visit(R);
  (*FS) << BM.declInScalarBuffer();

  // Write signals
  (*FS) << BlockSignals.str();

  // Write constant assignments
  (*FS) << ConstSignals.str();

  // State Machine
  (*FS) << BM.declFSMSignals();
  (*FS) << BM.declFSM();

  // Write component instantiations
  (*FS) << BlockComponents.str();

  // Schedule the created Operators
  runAsapScheduler(R);

  (*FS) << BM.declFooter();

  FS->close();

  return 0;
}

// The following methods create arithmetic cores. The block then instantiates
// them and takes care of the critical path.

/// \brief Generate Stream as BRAM with Addressgenerator
///

int Verilog::visit(Add &R) {
  VISIT_ONCE(R);
  const std::string BW = std::to_string(R.getBitWidth());
  
  const std::string Arch = std::to_string(conf::IntAdder_Arch);
  const std::string Opt = std::to_string(conf::IntAdder_OptObjective);
  const std::string SRL = std::to_string(conf::IntAdder_SRL);

  const std::string Name = "IntAdder_" + BW;

  const std::string RName = R.getUniqueName();

  std::stringstream FInst;

  FInst << "IntAdder" << " wIn=" << BW << " ";
  FInst << "arch=" << Arch << " ";
  FInst << "optObjective=" << Opt << " ";
  FInst << "SRL=" << SRL << " ";
  FInst << "name=" << Name << " ";
  FInst << "outputFile=" << Name << ".vhd" << " ";

  flopoco::addModule(RName, Name, FInst.str());

  // Add output signal
  Signal S(RName, R.getBitWidth(), Signal::Local, Signal::Wire);
  BlockSignals << S.getDefStr() << ";\n";

  // Instantiate component
  BlockComponents << "// " << RName << "\n";
  BlockComponents << Name << " " << Name << "_" << RName << "(\n";
  BlockComponents << Indent(1) << ".clk(clk)," << "\n";
  BlockComponents << Indent(1) << ".rst(rst)," << "\n";
  BlockComponents << Indent(1) << ".X(" << getOpName(R.getIn(0)) << ")," << "\n";
  BlockComponents << Indent(1) << ".Y(" << getOpName(R.getIn(1)) << ")," << "\n";
  BlockComponents << Indent(1) << ".Cin(" << RName << "_Cin" << ")," << "\n";
  BlockComponents << Indent(1) << ".R(" << RName << ")" << "\n";
  BlockComponents << ");\n";

  ConstSignals << "localparam " << RName << "_Cin = 0;" << "\n";

  return 0;
}

/// \brief Generate Subtractor.
///
/// TODO: Use adder with negated 2nd operand.
int Verilog::visit(Sub &R) {
  VISIT_ONCE(R);

  const std::string BW = std::to_string(R.getBitWidth());
  
  const std::string Arch = std::to_string(conf::IntAdder_Arch);
  const std::string Opt = std::to_string(conf::IntAdder_OptObjective);
  const std::string SRL = std::to_string(conf::IntAdder_SRL);

  const std::string Name = "IntAdder_" + BW;

  const std::string RName = R.getUniqueName();

  std::stringstream FInst;

  FInst << "IntAdder" << " wIn=" << BW << " ";
  FInst << "arch=" << Arch << " ";
  FInst << "optObjective=" << Opt << " ";
  FInst << "SRL=" << SRL << " ";
  FInst << "name=" << Name << " ";
  FInst << "outputFile=" << Name << ".vhd" << " ";

  flopoco::addModule(RName, Name, FInst.str());

  // Add output signal
  Signal S(RName, R.getBitWidth(), Signal::Local, Signal::Wire);
  BlockSignals << S.getDefStr() << ";\n";

  // Inverted input
  const std::string InvIn1Name = getOpName(R.getIn(1))+"_inv";
  Signal InvIn1(InvIn1Name, R.getIn(1)->getBitWidth(), Signal::Local, Signal::Wire);
  BlockSignals << InvIn1.getDefStr() << ";\n";
  BlockSignals << "assign " << InvIn1Name << " = ~" << getOpName(R.getIn(1)) << ";\n"; 

  // Instantiate component
  BlockComponents << "// " << RName << "\n";
  BlockComponents << Name << " " << Name << "_" << RName << "(\n";
  BlockComponents << Indent(1) << ".clk(clk)," << "\n";
  BlockComponents << Indent(1) << ".rst(rst)," << "\n";
  BlockComponents << Indent(1) << ".X(" << getOpName(R.getIn(0)) << ")," << "\n";
  BlockComponents << Indent(1) << ".Y(" << InvIn1Name << ")," << "\n";
  BlockComponents << Indent(1) << ".Cin(" << RName << "_Cin" << ")," << "\n";
  BlockComponents << Indent(1) << ".R(" << RName << ")" << "\n";
  BlockComponents << ");\n";

  ConstSignals << "localparam " << RName << "_Cin = 1;" << "\n";
  return 0;
}

int Verilog::visit(Mul &R) {
  VISIT_ONCE(R);
  assert(R.getIns().size() == 2);

  const std::string WX = std::to_string(R.getIn(0)->getBitWidth());
  const std::string WY = std::to_string(R.getIn(1)->getBitWidth());
  const std::string WOut = std::to_string(R.getBitWidth());

  bool isSigned = true;

  const std::string Name = "IntMultiplier_" + WX + "_" + WY + "_" + WOut;

  const std::string RName = R.getUniqueName();

  std::stringstream FInst;

  FInst << "IntMultiplier" << " wX=" << WX << " wY=" << WY << " WOut=" << WOut << " ";
  FInst << "signedIO=" << conf::to_string(isSigned) << " ";
  FInst << "name=" << Name << " ";
  FInst << "outputFile=" << Name << ".vhd" << " ";

  flopoco::addModule(RName, Name, FInst.str());

  // Add output signal
  Signal S(RName, R.getBitWidth(), Signal::Local, Signal::Wire);
  BlockSignals << S.getDefStr() << ";\n";

  // Instantiate component
  BlockComponents << "// " << RName << "\n";
  BlockComponents << Name << " " << Name << "_" << RName << "(\n";
  BlockComponents << Indent(1) << ".clk(clk)," << "\n";
  BlockComponents << Indent(1) << ".rst(rst)," << "\n";
  BlockComponents << Indent(1) << ".X(" << getOpName(R.getIn(0)) << ")," << "\n";
  BlockComponents << Indent(1) << ".Y(" << getOpName(R.getIn(1)) << ")," << "\n";
  BlockComponents << Indent(1) << ".R(" << RName << ")" << "\n";
  BlockComponents << ");\n";

  return 0;
}


int Verilog::visit(FAdd &R) {
  VISIT_ONCE(R);
  assert(R.getIns().size() == 2);

  const std::string isSub = "false";

  const std::string dualPath = conf::to_string(conf::FPAdd_DualPath);

  const std::string WE = std::to_string(R.getExponentBitWidth());
  const std::string WF = std::to_string(R.getMantissaBitWidth());

  const std::string Name = "FPAdd_" + WE + "_" + WF;

  const std::string RName = R.getUniqueName();

  std::stringstream FInst;

  FInst << "FPAdd" << " wE=" << WE << " wF=" << WF << " ";
  FInst << "sub=" << isSub << " ";
  FInst << "dualPath=" << dualPath << " ";
  FInst << "name=" << Name << " ";
  FInst << "outputFile=" << Name << ".vhd" << " ";

  flopoco::addModule(RName, Name, FInst.str());

  // Add output signal
  Signal S(RName, R.getBitWidth(), Signal::Local, Signal::Wire);
  BlockSignals << S.getDefStr() << ";\n";

  // Instantiate component
  BlockComponents << "// " << RName << "\n";
  BlockComponents << Name << " " << Name << "_" << RName << "(\n";
  BlockComponents << Indent(1) << ".clk(clk)," << "\n";
  BlockComponents << Indent(1) << ".rst(rst)," << "\n";
  BlockComponents << Indent(1) << ".X(" << getOpName(R.getIn(0)) << ")," << "\n";
  BlockComponents << Indent(1) << ".Y(" << getOpName(R.getIn(1)) << ")," << "\n";
  BlockComponents << Indent(1) << ".R(" << RName << ")" << "\n";
  BlockComponents << ");\n";

  return 0;
}

int Verilog::visit(FSub &R) {
  VISIT_ONCE(R);
  return 0;
}

int Verilog::visit(FMul &R) {
  VISIT_ONCE(R);

  assert(R.getIns().size() == 2);

  const std::string RName = R.getUniqueName();

  // Operands constant?
  const_p In0 = std::dynamic_pointer_cast<ConstVal>(R.getIn(0));
  const_p In1 = std::dynamic_pointer_cast<ConstVal>(R.getIn(1));

  if (In0 || In1) {
    assert(!(In0 && In1));
    const_p ConstOp = In0 ? In0 : In1;

    basefp_p VarOp = std::dynamic_pointer_cast<FPHW>(In0 ? R.getIn(1) : R.getIn(0));

    // TODO will fail when ine input is of type Port and the other is constVal
    assert(VarOp);

    const std::string WEin = std::to_string(VarOp->getExponentBitWidth());
    const std::string WFin = std::to_string(VarOp->getMantissaBitWidth());

    const std::string WEout = std::to_string(R.getExponentBitWidth());
    const std::string WFout = std::to_string(R.getMantissaBitWidth());

    // Use the name as it will be converted.
    const std::string constant = ConstOp->getName();

    std::string ConstOpName = ConstOp->getName();
    std::replace(ConstOpName.begin(), ConstOpName.end(), '.', '_');

    const std::string Name = "FPConstMult_" + WEout + "_" + WFout + "_" + ConstOpName;
    
    std::stringstream FInst;

    FInst << "FPConstMult" << " wE_in=" << WEin << " wF_in=" << WFin << " ";
    FInst << "wE_out=" << WEout << " wF_out=" << WFout << " ";
    FInst << "constant=\"" << constant << "\" ";
    FInst << "cst_width=0 ";
    FInst << "name=" << Name << " ";
    FInst << "outputFile=" << Name << ".vhd" << " ";

    flopoco::addModule(RName, Name, FInst.str());

    // Add output signal
    Signal S(RName, R.getBitWidth(), Signal::Local, Signal::Wire);
    BlockSignals << S.getDefStr() << ";\n";

    // Instantiate component
    BlockComponents << "// " << RName << "\n";
    BlockComponents << Name << " " << Name << "_" << RName << "(\n";
    BlockComponents << Indent(1) << ".clk(clk)," << "\n";
    BlockComponents << Indent(1) << ".rst(rst)," << "\n";
    BlockComponents << Indent(1) << ".X(" << VarOp->getUniqueName() << ")," << "\n";
    BlockComponents << Indent(1) << ".R(" << RName << ")" << "\n";
    BlockComponents << ");\n";

  } else {
    const std::string WE = std::to_string(R.getExponentBitWidth());
    const std::string WF = std::to_string(R.getMantissaBitWidth());

    const std::string WFout = WF;

    const std::string Name = "FPMult_" + WE + "_" + WF;

    std::stringstream FInst;

    FInst << "FPMult" << " wE=" << WE << " wF=" << WF << " ";
    FInst << "wFout=" << WFout << " ";
    FInst << "name=" << Name << " ";
    FInst << "outputFile=" << Name << ".vhd" << " ";

    flopoco::addModule(RName, Name, FInst.str());

    // Add output signal
    Signal S(RName, R.getBitWidth(), Signal::Local, Signal::Wire);
    BlockSignals << S.getDefStr() << ";\n";

    // Instantiate component
    BlockComponents << "// " << RName << "\n";
    BlockComponents << Name << " " << Name << "_" << RName << "(\n";
    BlockComponents << Indent(1) << ".clk(clk)," << "\n";
    BlockComponents << Indent(1) << ".rst(rst)," << "\n";
    BlockComponents << Indent(1) << ".X(" << R.getIn(0)->getUniqueName() << ")," << "\n";
    BlockComponents << Indent(1) << ".Y(" << R.getIn(1)->getUniqueName() << ")," <<"\n";
    BlockComponents << Indent(1) << ".R(" << RName << ")" << "\n";
    BlockComponents << ");\n";
  }

  return 0;
}
int Verilog::visit(UDiv &R) {
  VISIT_ONCE(R);
  return 0;
}
int Verilog::visit(SDiv &R) {
  VISIT_ONCE(R);
  return 0;
}
int Verilog::visit(FDiv &R) {
  VISIT_ONCE(R);
  return 0;
}
int Verilog::visit(URem &R) {
  VISIT_ONCE(R);
  return 0;
}
int Verilog::visit(SRem &R) {
  VISIT_ONCE(R);
  return 0;
}
int Verilog::visit(FRem &R) {
  VISIT_ONCE(R);
  return 0;
}
int Verilog::visit(Shl &R) {
  VISIT_ONCE(R);
  return 0;
}
int Verilog::visit(LShr &R) {
  VISIT_ONCE(R);
  return 0;
}
int Verilog::visit(AShr &R) {
  VISIT_ONCE(R);
  return 0;
}
int Verilog::visit(And &R) {
  VISIT_ONCE(R);
  return 0;
}
int Verilog::visit(Or &R) {
  VISIT_ONCE(R);
  return 0;
}
int Verilog::visit(Xor &R) {
  VISIT_ONCE(R);
  return 0;
}

int Verilog::visit(StreamPort &R) {
  VISIT_ONCE(R);
  return 0;
}

int Verilog::visit(StaticStreamIndex &R) {
  VISIT_ONCE(R);
  return 0;
}

int Verilog::visit(DynamicStreamIndex &R) {
  VISIT_ONCE(R);
  return 0;
}

void Verilog::runAsapScheduler(const Block &R) {
}


#undef DEBUG_TYPE
