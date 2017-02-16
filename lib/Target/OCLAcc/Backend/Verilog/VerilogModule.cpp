#include <memory>

#include "../../HW/Kernel.h"

#include "VerilogModule.h"


using namespace oclacc;

VerilogModule::VerilogModule(Component &C) : R(C) {
}

BlockModule::BlockModule(Block &B) : VerilogModule(B), R(B) {
}

KernelModule::KernelModule(Kernel &K) : VerilogModule(K), R(K) {
}

const std::string VerilogModule::declHeader() const {
  std::stringstream S;
  const Component::PortsTy &I = R.getIns();
  const Component::PortsTy &O = R.getOuts();

  S << "module " << R.getName() << "(\n";
  for (Component::PortsConstItTy P = I.begin(), E = I.end(); P != E; ++P) {
    S << I(1) << "input  ";

    unsigned B = (*P)->getBitWidth();
    std::stringstream BWS;
    if (B != 1)
      BWS << "[" << B-1 << ":0] ";
    
    std::string BW = BWS.str();

    S << BW << (*P)->getUniqueName() << ",\n";

    S << I(1) << "input wire  " << std::string(BW.length(), ' ' ) << (*P)->getUniqueName() << "_valid,\n";
    S << I(1) << "output reg" << std::string(BW.length(), ' ' ) << (*P)->getUniqueName() << "_ack";

    if (std::next(P) != E)
      S << ",\n";
  }

  if (I.size() != 0) {
    if (O.size() != 0)
      S << ",\n";
    else
      S << "\n";
  }

  for (Component::PortsConstItTy P = O.begin(), E = O.end(); P != E; ++P) {
    S << I(1) << "output ";

    unsigned B = (*P)->getBitWidth();
    std::stringstream BWS;
    if (B != 1)
      BWS << "[" << B-1 << ":0] ";
    std::string BW = BWS.str();

    S << BW << (*P)->getUniqueName() << ",\n";

    S << I(1) << "input  " << std::string(BW.length(), ' ' ) << (*P)->getUniqueName() << "_ack,\n";
    S << I(1) << "output " << std::string(BW.length(), ' ' ) << (*P)->getUniqueName() << "_valid";

    if (std::next(P) != E)
      S << ",\n";
  }
  S << "\n); // end ports\n";

  return S.str();
}

const std::string BlockModule::declEnable() const {
  std::stringstream S;
  
  S << "// block enable\n";
  S << "wire enable_" << R.getName() << ";\n";
  S << "assign enable_" << R.getName() << " = ";

  const Block::CondListTy &C = R.getConds();
  const Block::CondListTy &NC = R.getNegConds();

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
  S << "endmodule " << " // " << R.getUniqueName() << "\n";
  return S.str();
}

/// \brief Declare Wires for the Blocks' Ports
///
/// Iterate over all Blocks and creates wires for each port data. For
/// synchronization, also create <port>_valid and <port>_ack.
///
const std::string KernelModule::declBlockWires() const {
  std::stringstream S;
  for (block_p B : R.getBlocks()) {
    const Component::PortsTy &I = R.getIns();
    const Component::PortsTy &O = R.getOuts();

    Component::PortsTy IO;
    IO.insert(IO.end(), I.begin(), I.end());
    IO.insert(IO.end(), O.begin(), O.end());

    S << "// " << B->getUniqueName() << "\n";

    // first create wires for each port used as input and output
    for (Component::PortsConstItTy P = IO.begin(), E = IO.end(); P != E; ++P) {

      // should already be available as port, so skip it
      if (!(*P)->isPipelined()) continue;

      S << "wire ";

      unsigned B = (*P)->getBitWidth();
      if (B != 1)
        S << "[" << B-1 << ":0] ";

      S << (*P)->getUniqueName();

      S << ";\n";

      // Add valid and ack
      S << "wire " << (*P)->getUniqueName() << "_valid;\n";
      S << "wire " << (*P)->getUniqueName() << "_ack;\n";
    }
  }
  return S.str();
}

const std::string KernelModule::connectWires() const {
  for (block_p B : R.getBlocks()) {
    std::stringstream S;
    S << "// Connections for " << R.getUniqueName() << "\n";


    // It is sufficient to walk through all inputs.
    for (port_p P : R.getIns()) {

      // If there are more Inputs to this, there is already a Portmux instance
      // with an output port already named like the input port where it is used,
      // so we can skip it here.
      if (P->getIns().size() > 1)
        continue;

      // ScalarInputs and alike are directly used as input. Skip them as well.
      if (!P->isPipelined())
        continue;

      base_p I = P->getIn(0);

      S << "assign " << P->getUniqueName() << " = " << I->getUniqueName() << ";\n";
      S << "assign " << P->getUniqueName() << "_valid = " << I->getUniqueName() << "_valid;\n";
      S << "assign " << P->getUniqueName() << "_ack = " << I->getUniqueName() << "_ack;\n";
    }

    return S.str();
  }
}

const std::string KernelModule::instBlocks() const {
  std::stringstream S;
  for (block_p B : R.getBlocks()) {

    const Component::PortsTy &I = B->getIns();
    const Component::PortsTy &O = B->getOuts();

    Component::PortsTy IO;
    IO.insert(IO.end(), I.begin(), I.end());
    IO.insert(IO.end(), O.begin(), O.end());

    // Ports with multiple inputs have to be replaced with an port multiplexer to
    // decide at runtime, which port will be passed through.
    //
    bool printHead = true;
    for (const port_p P : I) {
      if (P->getIns().size() > 1) {
        Portmux PM(*P);
        if (printHead) {
          S << "// Muxer for Block " << B->getUniqueName() << "\n";
          printHead = false;
        }
        S << PM.instantiate();
      }
    }

    S << B->getName() << " " << B->getUniqueName() << "(\n";

    // Use can use the Port's name as it is unique. We already created wired for
    // each Port and we will connect them afterwards.
    // Input Ports with multiple inputs have to be connected to a multiplexer,
    // which is already generated.

    if (I.size() > 0) {
      S << I(1) << "// In\n";
      for (Component::PortsConstItTy P = I.begin(), E = I.end(); P != E; ++P) {
        S << I(1) << "." << (*P)->getUniqueName() << "(" << (*P)->getUniqueName() << "),\n";
        S << I(1) << "." << (*P)->getUniqueName() << "_valid(" << (*P)->getUniqueName() << "_valid),\n";
        S << I(1) << "." << (*P)->getUniqueName() << "_ack(" << (*P)->getUniqueName() << "_ack)";

        if (std::next(P) != E || O.size() > 0)
          S << ",\n";
      }
    }
    if (O.size() > 0) {
      S << I(1) << "// Out\n";

      for (Component::PortsConstItTy P = O.begin(), E = O.end(); P != E; ++P) {
        S << I(1) << "." << (*P)->getUniqueName() << "(" << (*P)->getUniqueName() << "),\n";
        S << I(1) << "." << (*P)->getUniqueName() << "_valid(" << (*P)->getUniqueName() << "_valid),\n";
        S << I(1) << "." << (*P)->getUniqueName() << "_ack(" << (*P)->getUniqueName() << "_ack)";
        if (std::next(P) != E)
          S << ",\n";
      }
    }

    S << "\n" << ");\n";
  }
  return S.str();
}

const std::string KernelModule::instStreams() const {
  std::stringstream S;

  for (streamport_p SP : R.getInStreams()) {
    S << SP->getUniqueName() << " BW: " << SP->getBitWidth() << "\n";

    for (streamindex_p SI : SP->getIndexList()) {
      S << "index " << SI->getUniqueName();

      if (SI->isStatic()) {
        staticstreamindex_p SSI = std::static_pointer_cast<StaticStreamIndex>(SI);
        S << " is static " << SSI->getIndex() << "\n";
      } else {
        dynamicstreamindex_p DSI = std::static_pointer_cast<DynamicStreamIndex>(SI);
        S << " is dynamic " << DSI->getIndex()->getUniqueName() << "\n";
      }
    }
  }

  for (streamport_p S : R.getOutStreams()) {
  }

  for (streamport_p S : R.getInOutStreams()) {
  }

  return S.str();
}
