#include <sstream>

#include "llvm/Support/ErrorHandling.h"

#include "../../HW/Kernel.h"
#include "../../HW/Arith.h"
#include "Naming.h"
#include "Macros.h"

using namespace oclacc;

extern Signal Clk;
extern Signal Rst;

const std::string oclacc::getOpName(const base_p P) {
  if (staticstreamindex_p SI = std::dynamic_pointer_cast<StaticStreamIndex>(P)) {
    return getOpName(*SI);
  } else if (dynamicstreamindex_p SC = std::dynamic_pointer_cast<DynamicStreamIndex>(P)) {
    return getOpName(*SC);
  } else if (scalarport_p SC = std::dynamic_pointer_cast<ScalarPort>(P)) {
  } else {
    return getOpName(*P);
  }
}

const std::string oclacc::getOpName(const StaticStreamIndex &P) {
  std::stringstream Name;

  const std::string PName = P.getUniqueName();
  const std::string StreamName = P.getStream()->getName();

  // Get numerical index
  StaticStreamIndex::IndexTy Index = P.getIndex();
  std::string Sign = "";
  if (Index < 0) {
    Index = -Index;
    Sign = "_";
  }

  Name << StreamName << "_" << PName << "_" << "const" << Sign << Index;

  return Name.str();
}

const std::string oclacc::getOpName(const DynamicStreamIndex &P) {
  std::stringstream Name;

  const std::string IndexName = P.getUniqueName();
  const std::string StreamName = P.getStream()->getName();

  const base_p Index = P.getIndex();
  const std::string ID = std::to_string(Index->getUID());

  Name << StreamName+"_"+IndexName+"_"+ID;

  return Name.str();
}

const std::string oclacc::getOpName(const HW &R) {
  return R.getUniqueName();
}

#if 0
const std::string oclacc:getOpName(const ScalarPort &P) {
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
  return PName;
}
#endif

// Used for delegation
const PortListTy oclacc::getInSignals(const scalarport_p P) {
  return getInSignals(*P);
}
const PortListTy oclacc::getOutSignals(const scalarport_p P) {
  return getOutSignals(*P);
}

/// \brief Return all signals for a specific port depending on its dynamic type
const PortListTy oclacc::getSignals(const block_p P) {
  return getSignals(*P);
}

const PortListTy oclacc::getSignals(const Block &R) {
  PortListTy L;
  L.push_back(Clk);
  L.push_back(Rst);

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

const PortListTy oclacc::getSignals(const kernel_p P) {
  return getSignals(*P);
}

const PortListTy oclacc::getSignals(const Kernel &R) {
  PortListTy L;

  L.push_back(Clk);
  L.push_back(Rst);
  
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

const PortListTy oclacc::getInSignals(const streamindex_p P) {
  if (P->isStatic()) {
    const staticstreamindex_p S = std::static_pointer_cast<StaticStreamIndex>(P);
    return getInSignals(*S);
  } else {
    const dynamicstreamindex_p S = std::static_pointer_cast<DynamicStreamIndex>(P);
    return getInSignals(*S);
  }
}

const PortListTy oclacc::getOutSignals(const streamindex_p P) {
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
const PortListTy oclacc::getInSignals(const ScalarPort &P) {
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

const PortListTy oclacc::getSignals(const streamport_p P) {
  return getSignals(*P);
}

const PortListTy oclacc::getSignals(const StreamPort &P) {
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

const PortListTy oclacc::getOutSignals(const ScalarPort &P) {
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

const PortListTy oclacc::getInSignals(const StaticStreamIndex &P) {
  PortListTy L;

  // TODO: Store bitwidth for constant addresses
  unsigned AddressWidth = P.getBitWidth();
  unsigned DataWidth = P.getStream()->getBitWidth();

  const std::string Name = getOpName(P);

  L.push_back(Signal(Name+"_address", AddressWidth, SignalDirection::Out, SignalType::Reg));
  L.push_back(Signal(Name, DataWidth, SignalDirection::In, SignalType::Wire));
  L.push_back(Signal(Name+"_valid", 1, SignalDirection::Out, SignalType::Reg));
  L.push_back(Signal(Name+"_ack", 1, SignalDirection::In, SignalType::Wire));

  return L;
}

const PortListTy oclacc::getInSignals(const DynamicStreamIndex &P) {
  PortListTy L;

  unsigned AddressWidth = 64;
  unsigned DataWidth = P.getStream()->getBitWidth();

  L.push_back(Signal(getOpName(P)+"_address", AddressWidth, SignalDirection::Out, SignalType::Reg));
  L.push_back(Signal(getOpName(P), DataWidth, SignalDirection::In, SignalType::Wire));
  L.push_back(Signal(getOpName(P)+"_valid", 1, SignalDirection::Out, SignalType::Reg));
  L.push_back(Signal(getOpName(P)+"_ack", 1, SignalDirection::In, SignalType::Wire));

  return L;
}

const PortListTy oclacc::getOutSignals(const StaticStreamIndex &P) {
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
  unsigned DataWidth = P.getStream()->getBitWidth();

  L.push_back(Signal(PName+"_"+IName+"_address", AddressWidth, SignalDirection::Out, SignalType::Reg));
  L.push_back(Signal(PName+"_"+IName, DataWidth, SignalDirection::Out, SignalType::Reg));
  L.push_back(Signal(PName+"_"+IName+"_valid", 1, SignalDirection::Out, SignalType::Reg));
  L.push_back(Signal(PName+"_"+IName+"_ack", 1, SignalDirection::In, SignalType::Wire));

  return L;
}

const PortListTy oclacc::getOutSignals(const DynamicStreamIndex &P) {
  PortListTy L;

  const streamport_p Stream = P.getStream();
  const std::string PName = Stream->getUniqueName();

  // Create pairs of address and data port for each store

  // Make sure that negativ array indices do not result in incollect signal
  // names

  const base_p Index = P.getIndex();
  const std::string IName = std::to_string(Index->getUID());

  unsigned AddressWidth = Index->getBitWidth();
  unsigned DataWidth = P.getStream()->getBitWidth();

  L.push_back(Signal(PName+"_"+IName+"_address", AddressWidth, SignalDirection::Out, SignalType::Reg));
  L.push_back(Signal(PName+"_"+IName, DataWidth, SignalDirection::Out, SignalType::Reg));
  L.push_back(Signal(PName+"_"+IName+"_valid", 1, SignalDirection::Out, SignalType::Reg));
  L.push_back(Signal(PName+"_"+IName+"_ack", 1, SignalDirection::In, SignalType::Wire));

  return L;
}

const std::string oclacc::createPortList(const PortListTy &Ports) {
  std::stringstream S;

  std::string Prefix = "";

  for (const Signal &P : Ports) {
    S << Prefix << Indent(1) << P.getDefStr();

    Prefix = ",\n";
  }
  return S.str();
}
