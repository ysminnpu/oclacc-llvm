#include <memory>
#include <sstream>

#include "../../HW/Kernel.h"

#include "Verilog.h"
#include "VerilogModule.h"
#include "Naming.h"


using namespace oclacc;

extern DesignFiles TheFiles;

VerilogModule::VerilogModule(Component &C) : Comp(C) {
}

BlockModule::BlockModule(Block &B) : VerilogModule(B), Comp(B) {
}


KernelModule::KernelModule(Kernel &K) : VerilogModule(K), Comp(K) {
}

const std::string KernelModule::declHeader() const {
  std::stringstream S;

  S << "module " << Comp.getName() << "(\n";

  Signal::SignalListTy Ports = getSignals(Comp);
  
  // All Kernel Ports are wires.
  for (Signal& S : Ports) {
    S.Type = Signal::Wire;
  }
  S << createPortList(Ports);

  S << "\n); // end ports\n";

  return S.str();
}

/// \brief Separate functions because StreamPorts have to be instantiated
/// differently
const std::string BlockModule::declHeader() const {
  std::stringstream S;

  S << "module " << Comp.getName() << "(\n";

  Signal::SignalListTy Ports = getSignals(Comp);
  S << createPortList(Ports);

  S << "\n); // end ports\n";

  return S.str();
}

const std::string BlockModule::declEnable() const {
  std::stringstream S;
  
  S << "// block enable\n";
  S << "wire enable_" << Comp.getName() << ";\n";
  S << "assign enable_" << Comp.getName() << " = ";

  const Block::CondListTy &C = Comp.getConds();
  const Block::CondListTy &NC = Comp.getNegConds();

  for (Block::CondConstItTy I=C.begin(), E=C.end(); I != E; ++I) {
    S << I->first->getUniqueName();
    if (std::next(I) != E) {
      S << " & ";
    }
  }
  for (Block::CondConstItTy I=NC.begin(), E=NC.end(); I != E; ++I) {
    S << "~" << I->first->getUniqueName();
    if (std::next(I) != E) {
      S << " & ";
    }
  }
  S << ";\n";
  S << "// end block enable\n";

  return S.str();
}

const std::string VerilogModule::declFooter() const {
  std::stringstream S;
  S << "endmodule " << " // " << Comp.getUniqueName() << "\n";
  return S.str();
}

/// \brief Declare Wires for the Blocks' Ports and connect them with the
/// Kernel inputs
///
/// Iterate over all Blocks' ports and create wires for each port.
/// Finally connect the kernel outsputs.
///
/// All non-pipelined ScalarPorts and StreamPorts already exist as kernel ports.
///
const std::string KernelModule::declBlockWires() const {
  std::stringstream S;

  for (block_p B : Comp.getBlocks()) {
    S << "// " << B->getUniqueName() << "\n";

    // Create wires for each port used as input and output and assign them to
    // their use in a component
    for (scalarport_p P : B->getInScalars()) {
      // Non-pipelined Inputs are the same as the Kernel's.
      if (!P->isPipelined())
        continue;

      // Make sure that the Block's ScalarInput is connected with the Kernel's
      HW::PortsSizeTy NumIns = P->getIns().size();
      assert(NumIns);

      if (NumIns == 1) {
        scalarport_p SI = std::static_pointer_cast<ScalarPort>(P->getIn(0));

        Signal::SignalListTy KernelSigs = getOutSignals(SI);
        Signal::SignalListTy BlockSigs = getInSignals(P);

        assert(KernelSigs.size() == BlockSigs.size());


        for (Signal::SignalListConstItTy
            KI = KernelSigs.begin(),
            BI = BlockSigs.begin(),
            KE = KernelSigs.end();
            KI != KE;
            ++KI, ++BI) {

          // Define Block's Port. All Ports are wires.
          Signal LocDef(BI->Name, BI->BitWidth, Signal::Local, Signal::Wire);
          S << LocDef.getDefStr() << ";\n";

          // Kernel->Block
          if (KI->Direction == Signal::Out) {
            S << "assign " << BI->Name << " = " << KI->Name << ";\n"; 
          } else if (KI->Direction == Signal::In) {
            S << "assign " << KI->Name << " = " << BI->Name << ";\n"; 
          }
        }
      } else 
        TODO("Add Muxer");
    }
  }
  return S.str();
}

/// \brief Instantiate blocks with ports
///
/// Use the Port's name as it is unique. We already created wires for
/// each Port and we will connect them afterwards.
///
/// - ScalarInputs with multiple sources are connected to a new multiplexer.
/// - Non-pipelined ScalarInputs need no synchronization signals.
/// - Streams have separate address ports for each store and load

const std::string KernelModule::instBlocks() {
  std::stringstream SBlock;
  for (block_p B : Comp.getBlocks()) {
    const std::string BName = B->getUniqueName();

    // Walk through all Ports of the Block.
    Signal::SignalListTy Ports = getSignals(B);

    // Blocks must have any input and output
    assert(Ports.size());

    // Block instance name
    SBlock << B->getName() << " " << B->getUniqueName() << "(\n";

    std::string Linebreak = "";
    for (const Signal &P : Ports) {
      const std::string PName = P.Name;

      SBlock << Linebreak << I(1) << "." << PName << "(" << PName << ")";
      Linebreak = ",\n";
    }

    SBlock << "\n" << ");\n";
  }
  return SBlock.str();
}
