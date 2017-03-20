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

VerilogModule::VerilogModule(Component &C) : Comp(C) {
}

BlockModule::BlockModule(Block &B) : VerilogModule(B), Comp(B) {
}

// Local Port functions
namespace {

// Components
const PortListTy getSignals(const block_p);
const PortListTy getSignals(const Block &);

const PortListTy getSignals(const kernel_p);
const PortListTy getSignals(const Kernel &);

const std::string createPortList(const PortListTy &);

// Scalars
const PortListTy getInSignals(const scalarport_p);
const PortListTy getOutSignals(const scalarport_p);

const PortListTy getInSignals(const ScalarPort &);
const PortListTy getOutSignals(const ScalarPort &);

// Streams
const PortListTy getSignals(const streamport_p);
const PortListTy getSignals(const StreamPort &);

const PortListTy getInSignals(const StaticStreamIndex &);
const PortListTy getInSignals(const DynamicStreamIndex &);

const PortListTy getOutSignals(const StaticStreamIndex &);
const PortListTy getOutSignals(const DynamicStreamIndex &);

// Used for delegation
const PortListTy getInSignals(const streamindex_p);

const PortListTy getOutSignals(const streamindex_p);

const PortListTy getInSignals(const scalarport_p P) {
  return getInSignals(*P);
}
const PortListTy getOutSignals(const scalarport_p P) {
  return getOutSignals(*P);
}

/// \brief Return all signals for a specific port depending on its dynamic type
const PortListTy getSignals(const block_p P) {
  return getSignals(*P);
}

const PortListTy getSignals(const Block &R) {
  PortListTy L;

  // Inputs
  for (const scalarport_p P : R.getInScalars()) {
    const PortListTy SISC = getInSignals(P);
    L.insert(std::end(L),std::begin(SISC), std::end(SISC)); 
  }
  for (const streamindex_p P : R.getInStreamIndices()) {
    const PortListTy SIST = getInSignals(P);
    L.insert(std::end(L),std::begin(SIST), std::end(SIST)); 
  }

  // Outouts
  for (const scalarport_p P : R.getOutScalars()) {
    const PortListTy SOSC = getOutSignals(P);
    L.insert(std::end(L),std::begin(SOSC), std::end(SOSC)); 
  }

  for (const streamindex_p P : R.getOutStreamIndices()) {
    const PortListTy SOST = getOutSignals(P);
    L.insert(std::end(L),std::begin(SOST), std::end(SOST)); 
  }

  return L;
}

const PortListTy getSignals(const kernel_p P) {
  return getSignals(*P);
}

const PortListTy getSignals(const Kernel &R) {
  PortListTy L;

  // Scalars
  for (const scalarport_p P : R.getInScalars()) {
    const PortListTy SISC = getInSignals(P);
    L.insert(std::end(L),std::begin(SISC), std::end(SISC)); 
  }

  for (const scalarport_p P : R.getOutScalars()) {
    const PortListTy SOSC = getOutSignals(P);
    L.insert(std::end(L),std::begin(SOSC), std::end(SOSC)); 
  }
  // Streams
  for (const streamport_p P : R.getStreams()) {
    const PortListTy SIST = getSignals(P);
    L.insert(std::end(L),std::begin(SIST), std::end(SIST)); 
  }

  return L;
}

const PortListTy getInSignals(const streamindex_p P) {
  if (P->isStatic()) {
    const staticstreamindex_p S = std::static_pointer_cast<StaticStreamIndex>(P);
    return getInSignals(*S);
  } else {
    const dynamicstreamindex_p S = std::static_pointer_cast<DynamicStreamIndex>(P);
    return getInSignals(*S);
  }
}

const PortListTy getOutSignals(const streamindex_p P) {
  if (P->isStatic()) {
    const staticstreamindex_p S = std::static_pointer_cast<StaticStreamIndex>(P);
    return getOutSignals(*S);
  } else {
    const dynamicstreamindex_p S = std::static_pointer_cast<DynamicStreamIndex>(P);
    return getOutSignals(*S);
  }
}


// Signal names:
// <Name>_<ID From>_<ID To> to minimize efforts to connect ports

/// \brief Return a list of all ScalarPorts
const PortListTy getInSignals(const ScalarPort &P) {
  PortListTy L;

  unsigned BitWidth = P.getBitWidth();

  const HW::PortsTy Ins = P.getIns();

  if (Ins.empty()) {
    // ScalarPorts without input are kernel inputs. As they are always added to
    // the EntryBlock, their name must be the same as in this Block, and thus we
    // have to get the id of the output instead of the input.
    const HW::PortsTy Outs = P.getOuts();
    assert(Outs.size() == 1);

    Identifiable::UIDTy OutsID = std::begin(Outs)->get()->getUID();

    const std::string PName = P.getName() + "_" + std::to_string(P.getUID()) + "_" + std::to_string(OutsID);

    L.push_back(Signal(PName,BitWidth,SignalDirection::In, SignalType::Wire));

    // Only pipelined ports have synchronization signals
    if (P.isPipelined()) {
      L.push_back(Signal(PName+"_valid", 1, SignalDirection::In, SignalType::Wire));
      L.push_back(Signal(PName+"_ack", 1, SignalDirection::Out, SignalType::Reg));
    }
  } else {
    for (base_p In : Ins) {
      const std::string PName = P.getName() + "_" + std::to_string(In->getUID()) + "_" + std::to_string(P.getUID());

      L.push_back(Signal(PName,BitWidth,SignalDirection::In, SignalType::Wire));

      // Only pipelined ports have synchronization signals
      if (P.isPipelined()) {
        L.push_back(Signal(PName+"_valid", 1, SignalDirection::In, SignalType::Wire));
        L.push_back(Signal(PName+"_ack", 1, SignalDirection::Out, SignalType::Reg));
      }
    }
  }

  return L;
}

const PortListTy getSignals(const streamport_p P) {
  return getSignals(*P);
}

const PortListTy getSignals(const StreamPort &P) {
  PortListTy L;

  const std::string PName = P.getUniqueName();

  // Loads
  for(staticstreamindex_p PI : P.getStaticLoads()) {
    const PortListTy Ports = getInSignals(PI);
    L.insert(std::end(L), std::begin(Ports), std::end(Ports));
  }
  for(dynamicstreamindex_p PI : P.getDynamicLoads()) {
    const PortListTy Ports = getInSignals(PI);
    L.insert(std::end(L), std::begin(Ports), std::end(Ports));
  }

  // Stores
  for(staticstreamindex_p PI : P.getStaticStores()) {
    const PortListTy Ports = getOutSignals(PI);
    L.insert(std::end(L), std::begin(Ports), std::end(Ports));
  }
  for(dynamicstreamindex_p PI : P.getDynamicStores()) {
    const PortListTy Ports = getOutSignals(PI);
    L.insert(std::end(L), std::begin(Ports), std::end(Ports));
  }

  return L;
}

const PortListTy getOutSignals(const ScalarPort &P) {
  PortListTy L;

  if (!P.isPipelined())
    llvm_unreachable("No non-pipelined OurScalars");

  unsigned BitWidth = P.getBitWidth();

  // No need to differentiate between Kernels and Blocks like in getInSignals()
  // as Kernels do not have scalar outputs.

  for (base_p Out : P.getOuts()) {
    const std::string PName = P.getName() + "_" + std::to_string(P.getUID()) + "_" + std::to_string(Out->getUID());

    L.push_back(Signal(PName, BitWidth, SignalDirection::Out, SignalType::Reg));

    L.push_back(Signal(PName+"_valid", 1, SignalDirection::Out, SignalType::Reg));

    L.push_back(Signal(PName+"_ack", 1, SignalDirection::In, SignalType::Wire));
  }

  return L;
}

const PortListTy getInSignals(const StaticStreamIndex &P) {
  PortListTy L;

  const std::string PName = P.getUniqueName();
  const std::string StreamName = P.getStream()->getName();

  StaticStreamIndex::IndexTy Index = P.getIndex();
  if (Index < 0) Index = -Index;
  const std::string IName = "const"+std::to_string(Index);

  // TODO Store bitwidth for constant addresses
  unsigned AddressWidth = 64;
  unsigned DataWidth = P.getBitWidth();

  L.push_back(Signal(StreamName+"_"+PName+"_"+IName+"_address", AddressWidth, SignalDirection::Out, SignalType::Reg));
  L.push_back(Signal(StreamName+"_"+PName+"_"+IName+"_data", DataWidth, SignalDirection::Out, SignalType::Reg));
  L.push_back(Signal(StreamName+"_"+PName+"_"+IName+"_valid", 1, SignalDirection::Out, SignalType::Reg));
  L.push_back(Signal(StreamName+"_"+PName+"_"+IName+"_ack", 1, SignalDirection::In, SignalType::Wire));

  return L;
}

const PortListTy getInSignals(const DynamicStreamIndex &P) {
  PortListTy L;

  const std::string IndexName = P.getUniqueName();
  const std::string StreamName = P.getStream()->getName();

  const base_p Index = P.getIndex();
  const std::string ID = std::to_string(Index->getUID());

  unsigned AddressWidth = Index->getBitWidth();
  unsigned DataWidth = P.getBitWidth();

  L.push_back(Signal(StreamName+"_"+IndexName+"_"+ID+"_address", AddressWidth, SignalDirection::Out, SignalType::Reg));
  L.push_back(Signal(StreamName+"_"+IndexName+"_"+ID+"_data", DataWidth, SignalDirection::Out, SignalType::Reg));
  L.push_back(Signal(StreamName+"_"+IndexName+"_"+ID+"_valid", 1, SignalDirection::Out, SignalType::Reg));
  L.push_back(Signal(StreamName+"_"+IndexName+"_"+ID+"_ack", 1, SignalDirection::In, SignalType::Wire));

  return L;
}

const PortListTy getOutSignals(const StaticStreamIndex &P) {
  PortListTy L;

  streamport_p Stream = P.getStream();
  const std::string PName = Stream->getUniqueName();

  // Create pairs of address and data port for each store

  // Make sure that negativ array indices do not result in incollect signal
  // names

  StaticStreamIndex::IndexTy Index = P.getIndex();
  if (Index < 0) {
    Index = -Index;
  }
  std::string IName = "const"+std::to_string(Index);

  // TODO Store bitwidth for constant addresses
  unsigned AddressWidth = 64;
  unsigned DataWidth = P.getBitWidth();

  L.push_back(Signal(PName+"_"+IName+"_address", AddressWidth, SignalDirection::Out, SignalType::Reg));
  L.push_back(Signal(PName+"_"+IName+"_data", DataWidth, SignalDirection::Out, SignalType::Reg));
  L.push_back(Signal(PName+"_"+IName+"_valid", 1, SignalDirection::Out, SignalType::Reg));
  L.push_back(Signal(PName+"_"+IName+"_ack", 1, SignalDirection::In, SignalType::Wire));

  return L;
}

const PortListTy getOutSignals(const DynamicStreamIndex &P) {
  PortListTy L;

  const streamport_p Stream = P.getStream();
  const std::string PName = Stream->getUniqueName();

  // Create pairs of address and data port for each store

  // Make sure that negativ array indices do not result in incollect signal
  // names

  const base_p Index = P.getIndex();
  const std::string IName = std::to_string(Index->getUID());

  unsigned AddressWidth = Index->getBitWidth();
  unsigned DataWidth = P.getBitWidth();

  L.push_back(Signal(PName+"_"+IName+"_address", AddressWidth, SignalDirection::Out, SignalType::Reg));
  L.push_back(Signal(PName+"_"+IName+"_data", DataWidth, SignalDirection::Out, SignalType::Reg));
  L.push_back(Signal(PName+"_"+IName+"_valid", 1, SignalDirection::Out, SignalType::Reg));
  L.push_back(Signal(PName+"_"+IName+"_ack", 1, SignalDirection::In, SignalType::Wire));

  return L;
}

const std::string createPortList(const PortListTy &Ports) {
  std::stringstream S;

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
  return S.str();
}

} // end ns

KernelModule::KernelModule(Kernel &K) : VerilogModule(K), Comp(K) {
}

const std::string KernelModule::declHeader() const {
  std::stringstream S;

  S << "module " << Comp.getName() << "(\n";

  PortListTy Ports = getSignals(Comp);
  S << createPortList(Ports);

  S << "\n); // end ports\n";

  return S.str();
}

/// \brief Separate functions because StreamPorts have to be instantiated
/// differently
const std::string BlockModule::declHeader() const {
  std::stringstream S;

  S << "module " << Comp.getName() << "(\n";

  PortListTy Ports = getSignals(Comp);
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

/// \brief Declare Wires for the Blocks' Ports
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

    // The EntryBlock must be skipped because Arguments also added to the first
    // Block.
    if (B->isEntryBlock())
      continue;

    // First create wires for each port used as input and output
    for (scalarport_p P : B->getInScalars()) {
      if (!P->isPipelined())
        continue;

      for (const Signal &Sig : getInSignals(P)) {
        S << Sig.getTypeStr() << " " << Sig.Name << ";\n";
      }
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
    PortListTy Ports = getSignals(B);

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
