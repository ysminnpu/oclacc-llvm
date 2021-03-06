#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/ErrorHandling.h"

#include <cstdio>
#include <cmath>
#include <sstream>
#include <cstdlib>
#include <set>
#include <unistd.h>
#include <memory>
#include <map>

#include "Verilog.h"
#include "FileHeader.h"
#include "Naming.h"
#include "VerilogMacros.h"

#include "HW/Writeable.h"
#include "HW/typedefs.h"
#include "VerilogModule.h"
#include "KernelModule.h"
#include "BlockModule.h"
#include "OperatorInstances.h"
#include "DesignFiles.h"
#include "Flopoco.h"
#include "BramArbiter.h"


#define DEBUG_TYPE "verilog"


using namespace oclacc;

const std::string conf::to_string(bool B) {
  return B? "true" : "false";
}

OperatorInstances TheOps;

// Local Port functions

Signal Clk("clk", 1, Signal::In, Signal::Wire);
Signal Rst("rst", 1, Signal::In, Signal::Wire);



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

  unsigned II = 0;

  std::string Filename = "top.v";
  FileTy FS = openFile(Filename);

  std::stringstream SS;

  SS << header();
  std::string Linebreak = "";

  SS << "module top(\n";
  for (const Signal &P : getSignals(R)) {
    SS << Linebreak << I(1) << P.getDefStr();
    Linebreak = ",\n";
  }

  SS << "\n" << ");\n";

  // instantiate Kernels
  for (kernel_p K : R.getKernels()) {
    Signal::SignalListTy Ports = getSignals(K);

    // Block instance name
    SS << K->getName() << " " << K->getUniqueName() << "(\n";

    Linebreak = "";
    for (const Signal &P : Ports) {
      const std::string PName = P.Name;

      SS << Linebreak << I(1) << "." << PName << "(" << PName << ")";
      Linebreak = ",\n";
    }

    SS << "\n" << ");\n";
  }

  (*FS) << SS.str();

  FS->close();

  // Visit all kernels
  super::visit(R);

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

  // Instantiate the Kernel
  KM = std::make_unique<KernelModule>(R);
  KM->addFile(KernelFilename);

  (*FS) << header();

  (*FS) << KM->declHeader();

  (*FS) << KM->declBlockWires();

  (*FS) << KM->instBlocks();

  // Local memory
  for (streamport_p P : R.getStreams()) {
    if (P->getAddressSpace() == ocl::AS_LOCAL)
      (*FS) << ip::declBramArbiter(P);
  }

  (*FS) << KM->declFooter();

  super::visit(R);

  FS->close();

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

  BM = std::make_unique<BlockModule>(R);

  BM->addFile(Filename);

  (*FS) << header();

  (*FS) << BM->declHeader();

  //if (R.isConditional())
  //  (*FS) << BM->declEnable();

  // Write constant assignments
  (*FS) << BM->declConstValues();

  // Create instances for all operations
  super::visit(R);

  (*FS) << BM->declPortControlSignals();

  // Write signals
  (*FS) << BM->declBlockSignals();

  // Write constant assignments
  (*FS) << BM->declConstSignals();

  // Write local operators
  (*FS) << BM->declLocalOperators();

  // Write assignments
  (*FS) << BM->declBlockAssignments();

  // Determine critical path
  BM->schedule(TheOps);

  // State Machine
  (*FS) << BM->declFSMSignals();
  (*FS) << BM->declFSM();

  // Store Signals
  (*FS) << BM->declStores();

  (*FS) << BM->declLoads();


  // Write component instantiations
  (*FS) << BM->declBlockComponents();

  (*FS) << BM->declFooter();

  BM->genTestBench();

  BM = nullptr;

  FS->close();

  return 0;
}

// Ports
int Verilog::visit(ScalarPort &R) {
  VISIT_ONCE(R);

  std::stringstream &BA = BM->getBlockAssignments();

  const std::string RName = getOpName(R);

  if (R.getParent()->isInScalar(R)) {

  } else if (R.getParent()->isOutScalar(R)) {

  } else
    llvm_unreachable("ScalarPort must be InScalar or OutScalar.");

  return 0;
}

int Verilog::visit(StreamPort &R) {
  VISIT_ONCE(R);
  return 0;
}

int Verilog::visit(LoadAccess &R) {
  VISIT_ONCE(R);

  const std::string RName = getOpName(R);

  TheOps.addOperator(RName, RName, 1);

  return 0;
}

int Verilog::visit(StoreAccess &R) {
  VISIT_ONCE(R);

  const std::string RName = getOpName(R);

  TheOps.addOperator(RName, RName, 1);

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

///  \brief Constant Shifts do not use a register
void Verilog::handleConstShift(const Shl &R, uint64_t C) {
  std::stringstream &BS = BM->getBlockSignals();
  std::stringstream &LO = BM->getLocalOperators();

  const base_p In0 = R.getIn(0);
  const std::string Op0 = getOpName(In0);
  const std::string Res = getOpName(R);

  const std::string RName = getOpName(R);

  Signal S(RName, R.getBitWidth(), Signal::Local, Signal::Wire);
  BS << S.getDefStr() << ";\n";

  uint64_t BitsOp = In0->getBitWidth();
  uint64_t Start = BitsOp - 1;
  if (BitsOp > C)
    Start -= C;

  LO << "assign " << Res << " = {{" << Op0 << "[" << Start << ":0]},{" << C << "{0}" << "}};\n";
}


// The following methods create arithmetic cores. The block then instantiates
// them and takes care of the critical path.

/// \brief Generate Stream as BRAM with Addressgenerator

void Verilog::handleInferableMath(const Arith &R, const std::string Op) {
  std::stringstream &BS = BM->getBlockSignals();
  std::stringstream &LO = BM->getLocalOperators();

  const std::string Op0 = getOpName(R.getIn(0));
  const std::string Op1 = getOpName(R.getIn(1));
  const std::string Res = getOpName(R);

  const std::string RName = getOpName(R);

  Signal S(RName, R.getBitWidth(), Signal::Local, Signal::Reg);
  BS << S.getDefStr() << ";\n";

  unsigned II = 0;
  LO << "always @(posedge clk)\n";
  BEGIN(LO);
  LO << Indent(II) << "if (rst==1)\n";
    LO << Indent(II+1) << Res << " = '0;\n";

  LO << Indent(II) << "else\n";
    LO << Indent(II+1) << Res << " = " << Op0 << " " << Op << " " << Op1 << ";\n";

  END(LO);

  TheOps.addOperator(RName, RName, 1);
}

int Verilog::visit(Add &R) {
  VISIT_ONCE(R);

  handleInferableMath(R, "+");

  super::visit(R);
  return 0;
}

/// \brief Generate Subtractor.
///
int Verilog::visit(Sub &R) {
  VISIT_ONCE(R);

  handleInferableMath(R, "-");

  super::visit(R);
  return 0;
}

std::string addRegister(const std::string &RName, unsigned BitWidth, std::stringstream &BS, std::stringstream &BC) {
  std::string OutName = RName + "_buf";

  Signal SB(OutName, BitWidth, Signal::Local, Signal::Wire);
  BS << SB.getDefStr() << ";\n";

  unsigned II = 0;
  BC << "always @(posedge clk)\n";
  BEGIN(BC);
  BC << Indent(II) << "if (rst==1)\n";
  BC << Indent(II+1) << RName << " = '0;\n";

  BC << Indent(II) << "else\n";
  BC << Indent(II+1) << RName << " = " << RName << "_buf;\n";
  END(BC);

  return OutName;
}

int Verilog::visit(Mul &R) {
  VISIT_ONCE(R);
  assert(R.getIns().size() == 2);

  std::stringstream &BS = BM->getBlockSignals();
  std::stringstream &BC = BM->getBlockComponents();

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

  unsigned Latency = flopoco::genModule(Name, FInst.str(), *BM);

  // Latency is typically 0, so add a register to the output.

  std::string OutName = RName;

  BC << "// " << RName << "\n";

  Signal::SignalType OutType = Signal::Wire;
  if (Latency == 0) {
    OutType = Signal::Reg;
    OutName = addRegister(RName, R.getBitWidth(), BS, BC);

    // Latency+1 for the register
    Latency++;
  }

  TheOps.addOperator(RName, Name, Latency);

  // Add output signal
  Signal S(RName, R.getBitWidth(), Signal::Local, OutType);
  BS << S.getDefStr() << ";\n";

  // Instantiate component
  BC << Name << " " << Name << "_" << RName << "(\n";
  BC << Indent(1) << ".clk(clk)," << "\n";
  BC << Indent(1) << ".rst(rst)," << "\n";
  BC << Indent(1) << ".X(" << getOpName(R.getIn(0)) << ")," << "\n";
  BC << Indent(1) << ".Y(" << getOpName(R.getIn(1)) << ")," << "\n";
  BC << Indent(1) << ".R(" << OutName << ")" << "\n";
  BC << ");\n";



  super::visit(R);
  return 0;
}


int Verilog::visit(FAdd &R) {
  VISIT_ONCE(R);
  assert(R.getIns().size() == 2);

  std::stringstream &BlockSignals = BM->getBlockSignals();
  std::stringstream &BlockComponents = BM->getBlockComponents();

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

  unsigned Latency = flopoco::genModule(Name, FInst.str(), *BM);
  TheOps.addOperator(RName, Name, Latency);

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

  super::visit(R);
  return 0;
}

int Verilog::visit(FSub &R) {
  VISIT_ONCE(R);
  return 0;
}

int Verilog::visit(FMul &R) {
  VISIT_ONCE(R);

  assert(R.getIns().size() == 2);

  std::stringstream &BlockSignals = BM->getBlockSignals();
  std::stringstream &BlockComponents = BM->getBlockComponents();

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

    unsigned Latency = flopoco::genModule(Name, FInst.str(), *BM);
    TheOps.addOperator(RName, Name, Latency);

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

    unsigned Latency = flopoco::genModule(Name, FInst.str(), *BM);
    TheOps.addOperator(RName, Name, Latency);

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

  super::visit(R);
  return 0;
}
int Verilog::visit(UDiv &R) {
  VISIT_ONCE(R);
  super::visit(R);
  return 0;
}
int Verilog::visit(SDiv &R) {
  VISIT_ONCE(R);
  super::visit(R);
  return 0;
}
int Verilog::visit(FDiv &R) {
  VISIT_ONCE(R);
  super::visit(R);
  return 0;
}
int Verilog::visit(URem &R) {
  VISIT_ONCE(R);
  super::visit(R);
  return 0;
}
int Verilog::visit(SRem &R) {
  VISIT_ONCE(R);
  super::visit(R);
  return 0;
}
int Verilog::visit(FRem &R) {
  VISIT_ONCE(R);
  super::visit(R);
  return 0;
}


int Verilog::visit(Shl &R) {
  VISIT_ONCE(R);

  if (const_p C = std::dynamic_pointer_cast<ConstVal>(R.getIn(1))) {
    assert(C->isStatic() && "Constant shifts must have a value");
    handleConstShift(R, C->getValue());
  } else
    report_fatal_error("Shift with dynamic width");

  return 0;
}
int Verilog::visit(LShr &R) {
  VISIT_ONCE(R);
  super::visit(R);
  return 0;
}
int Verilog::visit(AShr &R) {
  VISIT_ONCE(R);
  super::visit(R);
  return 0;
}
int Verilog::visit(And &R) {
  VISIT_ONCE(R);

  handleInferableMath(R, "&");

  super::visit(R);
  return 0;
}
int Verilog::visit(Or &R) {
  VISIT_ONCE(R);

  handleInferableMath(R, "|");

  super::visit(R);
  return 0;
}
int Verilog::visit(Xor &R) {
  VISIT_ONCE(R);

  handleInferableMath(R, "^");

  super::visit(R);
  return 0;
}

int Verilog::visit(IntCompare &R) {
  VISIT_ONCE(R);

  std::stringstream &BS = BM->getBlockSignals();
  std::stringstream &LO = BM->getLocalOperators();

  const std::string Op0 = getOpName(R.getIn(0));
  const std::string Op1 = getOpName(R.getIn(1));
  const std::string Res = getOpName(R);

  unsigned BitWidth = R.getBitWidth();
  unsigned BitWidthOp0 = R.getIn(0)->getBitWidth();
  unsigned BitWidthOp1 = R.getIn(1)->getBitWidth();

  Signal Diff(Res+"_diff", std::max(BitWidthOp0, BitWidthOp1), Signal::Local, Signal::Reg);

  Signal RSig(Res, BitWidth, Signal::Local, Signal::Reg);
  BS << RSig.getDefStr() << ";\n";

  LO << "always @(clk)\n";
  BEGIN(LO);
    LO << Indent(II) << Diff.getDefStr() << ";\n";

    LO << Indent(II) << "if (rst == 1)\n";
    BEGIN(LO);
    LO << Indent(II) << Res << "_diff <= " << Op0 << " - " << Op1 << ";\n";
    LO << Indent(II) << Res << " <= 0;\n";
    END(LO);

    LO << Indent(II) << "else\n";
    BEGIN(LO);

    LO << Indent(II) << Res << "_diff <= " << Op0 << " - " << Op1 << ";\n";
    LO << Indent(II) << Res << " <= 0;\n";

    switch (R.getPred()) {
      case llvm::CmpInst::Predicate::ICMP_EQ:
        LO << Indent(II) << "if (" << Res << "_diff == 0" << ") " << Res << " <= 1;\n";
        break;
      case llvm::CmpInst::Predicate::ICMP_NE:
        break;
      case llvm::CmpInst::Predicate::ICMP_UGT:
        break;
      case llvm::CmpInst::Predicate::ICMP_UGE:
        break;
      case llvm::CmpInst::Predicate::ICMP_ULT:
        break;
      case llvm::CmpInst::Predicate::ICMP_ULE:
        break;
      case llvm::CmpInst::Predicate::ICMP_SGT:
        LO << Indent(II) << "if (" << Res << "_diff > 0" << ") " << Res << " <= 1;\n";
        break;
      case llvm::CmpInst::Predicate::ICMP_SGE:
        LO << Indent(II) << "if (" << Res << "_diff >= 0" << ") " << Res << " <= 1;\n";
        break;
      case llvm::CmpInst::Predicate::ICMP_SLT:
        LO << Indent(II) << "if (" << Res << "_diff < 0" << ") " << Res << " <= 1;\n";
        break;
      case llvm::CmpInst::Predicate::ICMP_SLE:
        break;
      default:
        llvm_unreachable("Invalid predicate for icmp.");
    }
    END(LO);
  END(LO);

  super::visit(R);
  return 0;
}

int Verilog::visit(FPCompare &R) {
  VISIT_ONCE(R);
  super::visit(R);
  return 0;
}

int Verilog::visit(Mux &R) {
  VISIT_ONCE(R);

  std::stringstream &BS = BM->getBlockSignals();
  std::stringstream &LO = BM->getLocalOperators();

  const std::string RName = getOpName(R);
  Signal MSig(RName, R.getBitWidth(), Signal::Local, Signal::Reg);
  BS << MSig.getDefStr() << ";\n";

  unsigned II = 0;
  LO << "always @(*)\n";
    BEGIN(LO);
    LO << Indent(II) << RName << " <= '0;\n";
    for (const Mux::MuxInputTy &M : R.getIns()) {
      const std::string IName = M.first->getUniqueName();
      const std::string CName = M.second->getUniqueName();

      LO << Indent(II) << " if (" << CName << "_valid == 1 && " << CName << " == 1) " << RName << " <= " << IName << ";\n";
    }
    END(LO);

  super::visit(R);
  return 0;
}

int Verilog::visit(Barrier &R) {
  return 0;
}

#ifdef DEBUG_TYPE
#undef DEBUG_TYPE
#endif
