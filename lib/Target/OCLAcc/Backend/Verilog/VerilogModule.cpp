#include <memory>
#include <iomanip>
#include <sstream>

#include "../../HW/Kernel.h"

#include "Verilog.h"
#include "VerilogModule.h"


using namespace oclacc;

extern DesignFiles TheFiles;

Signal::Signal(std::string Name, unsigned BitWidth, SignalDirection Direction, SignalType Type) : Name(Name), BitWidth(BitWidth), Direction(Direction), Type(Type) {
}
const std::string Signal::getDirectionStr(void) const {
  return SignalDirection_S[Direction];
}
const std::string Signal::getTypeStr(void) const {
  return SignalType_S[Type];
}

VerilogModule::VerilogModule(Component &C) : R(C) {
}

BlockModule::BlockModule(Block &B) : VerilogModule(B), R(B) {
}

/// \brief Return all signals for a specific port depending on its dynamic type
const PortListTy oclacc::getComponentSignals(const component_p P) {
  return getComponentSignals(*P);
}

const PortListTy oclacc::getComponentSignals(const Component &R) {
  PortListTy L;

  // Inputs
  for (const scalarport_p P : R.getInScalars()) {
    const PortListTy SISC = getInPortSignals(P);
    L.insert(std::end(L),std::begin(SISC), std::end(SISC)); 
  }
  for (const streamport_p P : R.getInStreams()) {
    const PortListTy SIST = getInPortSignals(P);
    L.insert(std::end(L),std::begin(SIST), std::end(SIST)); 
  }

  // Outouts
  for (const scalarport_p P : R.getOutScalars()) {
    const PortListTy SOSC = getOutPortSignals(P);
    L.insert(std::end(L),std::begin(SOSC), std::end(SOSC)); 
  }

  for (const streamport_p P : R.getOutStreams()) {
    const PortListTy SOST = getOutPortSignals(P);
    L.insert(std::end(L),std::begin(SOST), std::end(SOST)); 
  }

  // InOutStreams
  for (const streamport_p P : R.getInOutStreams()) {
    const PortListTy SIIOST = getInPortSignals(P);
    L.insert(std::end(L),std::begin(SIIOST), std::end(SIIOST)); 
    const PortListTy SIOOST = getOutPortSignals(P);
    L.insert(std::end(L),std::begin(SIOOST), std::end(SIOOST)); 
  }

  return L;
}

const PortListTy oclacc::getInPortSignals(const port_p P) {
  if (P->isScalar()) {
    const scalarport_p S = std::static_pointer_cast<ScalarPort>(P);    
    return getInScalarPortSignals(*S);
  } else {
    const streamport_p S = std::static_pointer_cast<StreamPort>(P);    
    return getInStreamPortSignals(*S);
  }
}

const PortListTy oclacc::getOutPortSignals(const port_p P) {
  if (P->isScalar()) {
    const scalarport_p S = std::static_pointer_cast<ScalarPort>(P);    
    return getOutScalarPortSignals(*S);
  } else {
    const streamport_p S = std::static_pointer_cast<StreamPort>(P);    
    return getOutStreamPortSignals(*S);
  }
}


// Signal names:
// <Name>_<ID From>_<ID To> to minimize efforts to connect ports

/// \brief Return a list of all ScalarPorts
const PortListTy oclacc::getInScalarPortSignals(const ScalarPort &P) {
  PortListTy L;

  unsigned BitWidth = P.getBitWidth();
  
  for (base_p In : P.getIns()) {
    std::stringstream PNameS;
    PNameS << P.getName() << "_" << In->getUID() << "_" << P.getUID();
    const std::string PName = PNameS.str();

    L.push_back(Signal(PName,BitWidth,SignalDirection::In, SignalType::Wire));

    // Only pipelined ports have synchronization signals
    if (P.isPipelined()) {
      L.push_back(Signal(PName+"_valid", 1, SignalDirection::In, SignalType::Wire));
      L.push_back(Signal(PName+"_ack", 1, SignalDirection::Out, SignalType::Reg));
    }
  }

  return L;
}

const PortListTy oclacc::getInStreamPortSignals(const StreamPort &P) {
  PortListTy L;

  const std::string PName = P.getUniqueName();

  // Walk through all loads and create address signals. StaticIndexes also
  // require an address signal, which is set to the value in the wire
  // declaration.
  for(staticstreamindex_p PI : P.getStaticLoads()) {
    // Static loads have fixed addresses stored in the memory controller
    const std::string PIName = std::to_string(PI->getUID());
    unsigned AddressWidth = PI->getBitWidth();
    unsigned DataWidth = P.getBitWidth();

    L.push_back(Signal(PName+"_"+PIName+"_address", AddressWidth, SignalDirection::Out, SignalType::Reg));
    L.push_back(Signal(PName+"_"+PIName+"_data", DataWidth, SignalDirection::In, SignalType::Wire));
    L.push_back(Signal(PName+"_"+PIName+"_valid", 1, SignalDirection::In, SignalType::Wire));
    L.push_back(Signal(PName+"_"+PIName+"_ack", 1, SignalDirection::Out, SignalType::Reg));
  }
  for(dynamicstreamindex_p PI : P.getDynamicLoads()) {
    const base_p Index = PI->getIndex();
    const std::string PIName = std::to_string(PI->getUID());

    unsigned AddressWidth = Index->getBitWidth();
    unsigned DataWidth = P.getBitWidth();

    L.push_back(Signal(PName+"_"+PIName+"_address", AddressWidth, SignalDirection::Out, SignalType::Reg));
    L.push_back(Signal(PName+"_"+PIName+"_data", DataWidth, SignalDirection::In, SignalType::Wire));
    L.push_back(Signal(PName+"_"+PIName+"_valid", 1, SignalDirection::In, SignalType::Wire));
    L.push_back(Signal(PName+"_"+PIName+"_ack", 1, SignalDirection::Out, SignalType::Reg));
  }

  return L;
}

const PortListTy oclacc::getOutScalarPortSignals(const ScalarPort &P) {
  PortListTy L;

  if (!P.isPipelined())
    llvm_unreachable("No non-pipelined OurScalars");

  unsigned BitWidth = P.getBitWidth();

  for (base_p Out : P.getOuts()) {
    std::stringstream PNameS;
    PNameS << P.getName() << "_" << P.getUID() << "_" << Out->getUID();

    const std::string PName = PNameS.str();
    L.push_back(Signal(PName, BitWidth, SignalDirection::Out, SignalType::Reg));

    L.push_back(Signal(PName+"_valid", 1, SignalDirection::Out, SignalType::Reg));

    // Multiple outputs need multiple ack signals
    for (base_p O : P.getOuts()) {
      L.push_back(Signal(PName+"_"+O->getUniqueName()+"_ack", 1, SignalDirection::In, SignalType::Wire));
    }
  }

  return L;
}


const PortListTy oclacc::getOutStreamPortSignals(const StreamPort &P) {
  PortListTy L;

  const std::string PName = P.getUniqueName();

  // Create pairs of address and data port for each store
  for(staticstreamindex_p PI : P.getStaticStores()) {
    // Make sure that negativ array indices do not result in incollect signal
    // names
    StaticStreamIndex::IndexTy Index = PI->getIndex();
    if (Index < 0) Index = -Index;
    const std::string IName = "const"+std::to_string(Index);

    unsigned AddressWidth = PI->getBitWidth();
    unsigned DataWidth = P.getBitWidth();

    L.push_back(Signal(PName+"_"+IName+"_address", AddressWidth, SignalDirection::Out, SignalType::Reg));
    L.push_back(Signal(PName+"_"+IName+"_data", DataWidth, SignalDirection::Out, SignalType::Reg));
    L.push_back(Signal(PName+"_"+IName+"_valid", 1, SignalDirection::Out, SignalType::Reg));
    L.push_back(Signal(PName+"_"+IName+"_ack", 1, SignalDirection::In, SignalType::Wire));
  }
  for(dynamicstreamindex_p PI : P.getDynamicStores()) {
    const base_p Index = PI->getIndex();
    const std::string IName = std::to_string(Index->getUID());

    unsigned AddressWidth = Index->getBitWidth();
    unsigned DataWidth = P.getBitWidth();

    L.push_back(Signal(PName+"_"+IName+"_address", AddressWidth, SignalDirection::Out, SignalType::Reg));
    L.push_back(Signal(PName+"_"+IName+"_data", DataWidth, SignalDirection::Out, SignalType::Reg));
    L.push_back(Signal(PName+"_"+IName+"_valid", 1, SignalDirection::Out, SignalType::Reg));
    L.push_back(Signal(PName+"_"+IName+"_ack", 1, SignalDirection::In, SignalType::Wire));
  }

  return L;
}

KernelModule::KernelModule(Kernel &K) : VerilogModule(K), R(K) {
}

const std::string VerilogModule::declHeader() const {
  std::stringstream S;

  PortListTy Ports = getComponentSignals(R);

  S << "module " << R.getName() << "(\n";

  std::string Prefix = "";
  unsigned Lastwidth = 15;

  for (const Signal &P : Ports) {
    S << Prefix << I(1) << std::setw(6) << std::left << P.getDirectionStr() << " " << std::setw(4) << P.getTypeStr() << " ";

    unsigned B = P.BitWidth;

    // Print width columnwise
    if (B!=1) {
      std::stringstream BWS;
      BWS << "[" << B-1 << ":0]";

      S << std::setw(6) << BWS.str();
    } else
      S << std::string(6, ' ');


    S << " " << P.Name;

    Prefix = ",\n";
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
/// Iterate over all Blocks' ports and create wires for each port.
/// Finally connect the kernel outsputs.
///
const std::string KernelModule::declBlockWires() const {
  std::stringstream S;
#if 0
  for (block_p B : R.getBlocks()) {
    S << "// " << B->getUniqueName() << "\n";

    const Component::PortsTy IO = B.getPorts();
    // Blocks must not have no input
    assert(IO.size());

    // First create wires for each port used as input and output
    for (port_p P : IO) {

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
#endif
  return S.str();
}

/// \brief Walk through all Blocks and connect inputs.
///
/// For Scalars, connect valid and ack. Streams also have an address. Skip
/// Kernel ports (must be StreamPorts) as they already exist in the Top module.
const std::string KernelModule::connectWires() const {
  std::stringstream S;


#if 0
  S << "// Port connections\n";

  // It is sufficient to walk through all inputs.
  for (block_p B : R.getBlocks()) {
    S << "// " << curr_block->getUniqueName() << "\n";

    // Ports
    if (P->getIns().size() > 1)

    // We only have a single input
    base_p PI = P->getIn(0);
    for (const Component::PortsTy &P : B->getIns()) {
    }

    PortListTy OutPorts = getOutPortSignals(B);
    PortListTy InPorts = getInPortSignals(B);
  }

  for (port_p P : I) {
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
#endif

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
  for (block_p B : R.getBlocks()) {
    const std::string BName = B->getUniqueName();

    // Walk through all Ports of the Block.
    PortListTy Ports = getComponentSignals(B);

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
