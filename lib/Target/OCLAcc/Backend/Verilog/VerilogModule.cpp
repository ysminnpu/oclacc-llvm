#include <memory>

#include "../../HW/Kernel.h"

#include "Verilog.h"
#include "VerilogModule.h"


using namespace oclacc;

extern DesignFiles TheFiles;


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
    S << I(1) << "input  wire ";

    unsigned B = (*P)->getBitWidth();
    std::stringstream BWS;
    if (B != 1)
      BWS << "[" << B-1 << ":0] ";
    
    std::string BW = BWS.str();

    S << BW << (*P)->getUniqueName() << ",\n";

    S << I(1) << "input  wire " << std::string(BW.length(), ' ' ) << (*P)->getUniqueName() << "_valid,\n";
    S << I(1) << "output reg  " << std::string(BW.length(), ' ' ) << (*P)->getUniqueName() << "_ack";

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
    S << I(1) << "output reg  ";

    unsigned B = (*P)->getBitWidth();
    std::stringstream BWS;
    if (B != 1)
      BWS << "[" << B-1 << ":0] ";
    std::string BW = BWS.str();

    S << BW << (*P)->getUniqueName() << ",\n";

    S << I(1) << "input  wire " << std::string(BW.length(), ' ' ) << (*P)->getUniqueName() << "_ack,\n";
    S << I(1) << "output reg  " << std::string(BW.length(), ' ' ) << (*P)->getUniqueName() << "_valid";

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
/// Iterate over all Blocks and create wires for each port data. For
/// synchronization, also create <port>_valid and <port>_ack.
///
const std::string KernelModule::declBlockWires() const {
  std::stringstream S;
  for (block_p B : R.getBlocks()) {
    const Component::PortsTy I = B->getIns();
    const Component::PortsTy O = B->getOuts();

    dbgs() << "Block " << B->getName() << " has " << I.size() << " Ins and " << O.size() << " Outs\n";

    Component::PortsTy IO;
    IO.insert(IO.end(), I.begin(), I.end());
    IO.insert(IO.end(), O.begin(), O.end());

    S << "// " << B->getUniqueName() << "\n";

    // first create wires for each port used as input and output
    for (port_p P : IO) {

      // Skip Streams as they are directly connected
      if (!P->isPipelined())
        continue;

      S << "wire ";

      unsigned B = P->getBitWidth();
      if (B != 1)
        S << "[" << B-1 << ":0] ";

      S << P->getUniqueName();

      S << ";\n";

      // Add valid and ack
      S << "wire " << P->getUniqueName() << "_valid;\n";
      S << "wire " << P->getUniqueName() << "_ack;\n";
    }
  }
  return S.str();
}

/// \brief Walk through all Blocks and connect inputs with outputs. 
///
/// For Scalars, connect valid and ack. Streams also have an address. Skip
/// Kernel ports as they already exist in the Top module.
const std::string KernelModule::connectWires() const {
  std::stringstream S;

  S << "// Assign block ports\n";

  Component::PortsTy I;

  // It is sufficient to walk through all inputs.
  for (block_p B : R.getBlocks()) {
    const Component::PortsTy PI = B->getIns();
    I.insert(I.end(), PI.begin(), PI.end());
  }

  component_p curr_block;

  for (port_p P : I) {
    // Skip Streams as they are directly connected
    if (!P->isPipelined()) 
      continue;

    // Print header between blocks
    component_p parent = P->getParent();
    assert(parent);

    if (curr_block != parent) {
      curr_block = parent;
      S << "// " << curr_block->getUniqueName() << "\n";
    }

    // If there are more Inputs to this, there is already a Portmux instance
    // with an output port already named like the input port where it is used,
    // so we can skip it here.
    if (P->getIns().size() > 1)
      continue;

    // We only have a single input
    base_p PI = P->getIn(0);
    if (!PI) dbgs() << P->getUniqueName() << " does not have an input\n";
    assert(PI);

    S << "assign " << P->getUniqueName() << " = " << PI->getUniqueName() << ";\n";
    S << "assign " << P->getUniqueName() << "_valid = " << PI->getUniqueName() << "_valid;\n";
    S << "assign " << P->getUniqueName() << "_ack = " << PI->getUniqueName() << "_ack;\n";
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
  std::stringstream SMux;
  std::stringstream SBlock;
  for (block_p B : R.getBlocks()) {
    const std::string BName = B->getUniqueName();

    const Component::PortsTy &I = B->getIns();
    const Component::PortsTy &O = B->getOuts();

    Component::PortsTy IO;
    IO.insert(IO.end(), I.begin(), I.end());
    IO.insert(IO.end(), O.begin(), O.end());

    // Walk through all Ports of the Block.



    const Component::ScalarsTy &ISC = B->getInScalars();
    const Component::ScalarsTy &OSC = B->getOutScalars();
    const Component::StreamsTy &IST = B->getInStreams();
    const Component::StreamsTy &OST = B->getOutStreams();

    // Blocks must have any input and output
    assert(!ISC.empty() || !IST.empty());
    if (OSC.empty() || OST.empty()) {
      dbgs() << BName << "\n";
    }
    assert(!OSC.empty() || !OST.empty());
    

    // Block instance name
    SBlock << B->getName() << " " << B->getUniqueName() << "(\n";

    bool print_head = true;
    std::string Linebreak = "";
    for (const scalarport_p P : ISC) {
      const std::string PName = P->getUniqueName();
      if (print_head) {
        SBlock << I(1) << "// In Scalars\n";
        print_head = false;
      }

      SBlock << Linebreak << I(1) << "." << PName << "(" << PName << ")";

      if (!P->isPipelined()) {
        // No synchronization ports are needed
      } else {
        if (P->getIns().size() == 1) {
          // Normal synchronization signals
          SBlock << ",\n" << I(1) << "." << PName << "_valid(" << PName << "_valid),\n";
          SBlock << I(1) << "." << PName << "_ack(" << PName << "_ack)";
        } else {
          // If the Input has multiple Inputs (same Value from different BBs), we
          // need a Multiplexer to select the correct value to be used at runtime.
          Portmux PM(*P);
          SMux << "// Muxer for Block " << BName << " Input " << PName << "\n";
          SMux << PM.instantiate();

          TheFiles.addFile(PM.getFileName());
        }
      }
      Linebreak = ",\n";
    }

    for (const streamport_p P : IST) {
      const std::string PName = P->getUniqueName();
      if (print_head) {
        SBlock << I(1) << "// In Streams\n";
        print_head = false;
      }
      // Walk through all loads and create address signals
      for(staticstreamindex_p PI : P->getStaticLoads()) {
        int64_t Index = PI->getIndex();
        SBlock << Linebreak << I(1) << "." << PName << "_data(" << PName << "_data)";
      }
      for(dynamicstreamindex_p PI : P->getDynamicLoads()) {
        const base_p Index = PI->getIndex();
        const std::string IName = Index->getUniqueName();

        SBlock << Linebreak << I(1) << "." << PName << "_" << IName << "_address(" << PName << "_" << IName << "_address),\n";
        SBlock << I(1) << "." << PName << "_" << IName << "_data(" << PName << "_" << IName << "_data)";
      }
      Linebreak = ",\n";
    }

    print_head = true;
    for (const scalarport_p P: OSC) {
      if (print_head) {
        SBlock << I(1) << "// Out Scalars\n";
        print_head = false;
      }
      Linebreak = ",\n";
    }

    print_head = true;
    for (const streamport_p P : OST) {
      if (print_head) {
        SBlock << I(1) << "// Out Streams\n";
        print_head = false;
      }
      // Create pairs of address and data port for each store
      Linebreak = ",\n";
    }

#if 0

    if (I.size() > 0) {
      S << I(1) << "// In\n";
      for (Component::PortsConstItTy P = I.begin(), E = I.end(); P != E; ++P) {

        S << I(1) << "." << (*P)->getUniqueName() << "(" << (*P)->getUniqueName() << "),\n";

        if ((*P)->isScalar()) {
          scalarport_p SP = std::static_pointer_cast<ScalarPort>(*P);

          if (SP->isPipelined()) {
            S << I(1) << "." << (*P)->getUniqueName() << "_valid(" << (*P)->getUniqueName() << "_valid),\n";
            S << I(1) << "." << (*P)->getUniqueName() << "_ack(" << (*P)->getUniqueName() << "_ack)";
          }
        } else {
          // Streams are added as Input or Output of a Block but they have
          // neither.
        }

        if (std::next(P) != E || O.size() > 0)
          S << ",\n";
      }
    }
    if (O.size() > 0) {
      S << I(1) << "// Out\n";

      for (Component::PortsConstItTy P = O.begin(), E = O.end(); P != E; ++P) {

        S << I(1) << "." << (*P)->getUniqueName() << "(" << (*P)->getUniqueName() << "),\n";

        if ((*P)->isScalar()) {
          scalarport_p SP = std::static_pointer_cast<ScalarPort>(*P);

          if (SP->isPipelined()) {
            S << I(1) << "." << (*P)->getUniqueName() << "_valid(" << (*P)->getUniqueName() << "_valid),\n";
            S << I(1) << "." << (*P)->getUniqueName() << "_ack(" << (*P)->getUniqueName() << "_ack)";
          }
        } else {
        }
          dbgs() << "Out " << (*P)->dump() << "\n";

        S << I(1) << "." << (*P)->getUniqueName() << "(" << (*P)->getUniqueName() << "),\n";
        S << I(1) << "." << (*P)->getUniqueName() << "_valid(" << (*P)->getUniqueName() << "_valid),\n";
        S << I(1) << "." << (*P)->getUniqueName() << "_ack(" << (*P)->getUniqueName() << "_ack)";
        if (std::next(P) != E)
          S << ",\n";
      }
    }
#endif

    SBlock << "\n" << ");\n";

  }
  SMux << SBlock.str();
  return SMux.str();
}

const std::string KernelModule::instStreams() const {
  std::stringstream S;

  for (streamport_p SP : R.getInStreams()) {
    //S << SP->getUniqueName() << " BW: " << SP->getBitWidth() << "\n";

    for (streamindex_p SI : SP->getIndexList()) {
      //S << "index " << SI->getUniqueName();

      if (SI->isStatic()) {
        staticstreamindex_p SSI = std::static_pointer_cast<StaticStreamIndex>(SI);
        //S << " is static " << SSI->getIndex() << "\n";
      } else {
        dynamicstreamindex_p DSI = std::static_pointer_cast<DynamicStreamIndex>(SI);
        //S << " is dynamic " << DSI->getIndex()->getUniqueName() << "\n";
      }
    }
  }

  for (streamport_p S : R.getOutStreams()) {
  }

  for (streamport_p S : R.getInOutStreams()) {
  }

  return S.str();
}
