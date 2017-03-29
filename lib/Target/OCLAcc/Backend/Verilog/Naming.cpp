#include <sstream>

#include "llvm/Support/ErrorHandling.h"

#include "../../HW/Kernel.h"
#include "../../HW/Arith.h"
#include "Naming.h"
#include "VerilogMacros.h"

using namespace oclacc;

extern Signal Clk;
extern Signal Rst;

const std::string oclacc::getOpName(const base_p P) {
  if (staticstreamindex_p SI = std::dynamic_pointer_cast<StaticStreamIndex>(P)) {
    return getOpName(*SI);
  } else if (dynamicstreamindex_p SC = std::dynamic_pointer_cast<DynamicStreamIndex>(P)) {
    return getOpName(*SC);
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

      L.push_back(Signal(PName,BitWidth,Signal::In, Signal::Wire));

      // Only pipelined ports have synchronization signals
      if (P.isPipelined()) {
        L.push_back(Signal(PName+"_valid", 1, Signal::In, Signal::Wire));
        L.push_back(Signal(PName+"_ack", 1, Signal::Out, Signal::Reg));
      }
    }

  }
  return PName;
}
#endif

// Used for delegation
const Signal::SignalListTy oclacc::getInSignals(const scalarport_p P) {
  return getInSignals(*P);
}
const Signal::SignalListTy oclacc::getOutSignals(const scalarport_p P) {
  return getOutSignals(*P);
}

/// \brief Return all signals for a specific port depending on its dynamic type
const Signal::SignalListTy oclacc::getSignals(const block_p P) {
  return getSignals(*P);
}

const Signal::SignalListTy oclacc::getSignals(const Block &R) {
  Signal::SignalListTy L;
  L.push_back(Clk);
  L.push_back(Rst);

  // Inputs
  for (const scalarport_p P : R.getInScalars()) {
    const Signal::SignalListTy SISC = getInSignals(P);
    L.insert(std::end(L),std::begin(SISC), std::end(SISC)); 
  }
  for (const streamindex_p P : R.getInStreamIndices()) {
    const Signal::SignalListTy SIST = getInSignals(P);
    L.insert(std::end(L),std::begin(SIST), std::end(SIST)); 
  }

  // Outouts
  for (const scalarport_p P : R.getOutScalars()) {
    const Signal::SignalListTy SOSC = getOutSignals(P);
    L.insert(std::end(L),std::begin(SOSC), std::end(SOSC)); 
  }

  for (const streamindex_p P : R.getOutStreamIndices()) {
    const Signal::SignalListTy SOST = getOutSignals(P);
    L.insert(std::end(L),std::begin(SOST), std::end(SOST)); 
  }

  return L;
}

const Signal::SignalListTy oclacc::getSignals(const kernel_p P) {
  return getSignals(*P);
}

const Signal::SignalListTy oclacc::getSignals(const Kernel &R) {
  Signal::SignalListTy L;

  L.push_back(Clk);
  L.push_back(Rst);
  
  // Scalars
  for (const scalarport_p P : R.getInScalars()) {
    const Signal::SignalListTy SISC = getInSignals(P);
    L.insert(std::end(L),std::begin(SISC), std::end(SISC)); 
  }

  for (const scalarport_p P : R.getOutScalars()) {
    const Signal::SignalListTy SOSC = getOutSignals(P);
    L.insert(std::end(L),std::begin(SOSC), std::end(SOSC)); 
  }
  // Streams
  for (const streamport_p P : R.getStreams()) {
    const Signal::SignalListTy SIST = getSignals(P);
    L.insert(std::end(L),std::begin(SIST), std::end(SIST)); 
  }

  return L;
}

const Signal::SignalListTy oclacc::getInSignals(const streamindex_p P) {
  if (P->isStatic()) {
    const staticstreamindex_p S = std::static_pointer_cast<StaticStreamIndex>(P);
    return getInSignals(*S);
  } else {
    const dynamicstreamindex_p S = std::static_pointer_cast<DynamicStreamIndex>(P);
    return getInSignals(*S);
  }
}

const Signal::SignalListTy oclacc::getOutSignals(const streamindex_p P) {
  if (P->isStatic()) {
    const staticstreamindex_p S = std::static_pointer_cast<StaticStreamIndex>(P);
    return getOutSignals(*S);
  } else {
    const dynamicstreamindex_p S = std::static_pointer_cast<DynamicStreamIndex>(P);
    return getOutSignals(*S);
  }
}


/// \brief Return a list of all ScalarPorts
const Signal::SignalListTy oclacc::getInSignals(const ScalarPort &P) {
  Signal::SignalListTy L;

  unsigned BitWidth = P.getBitWidth();

  const HW::PortsTy Ins = P.getIns();

  const std::string PName = getOpName(P);

  L.push_back(Signal(PName,BitWidth,Signal::In, Signal::Wire));

  // Only pipelined ports have synchronization signals
  if (P.isPipelined()) {
    L.push_back(Signal(PName+"_valid", 1, Signal::In, Signal::Wire));
    L.push_back(Signal(PName+"_ack", 1, Signal::Out, Signal::Reg));
  }

  return L;
}

const Signal::SignalListTy oclacc::getSignals(const streamport_p P) {
  return getSignals(*P);
}

const Signal::SignalListTy oclacc::getSignals(const StreamPort &P) {
  Signal::SignalListTy L;

  const std::string PName = getOpName(P);

  // Loads
  for(staticstreamindex_p PI : P.getStaticLoads()) {
    const Signal::SignalListTy Ports = getInSignals(PI);
    L.insert(std::end(L), std::begin(Ports), std::end(Ports));
  }
  for(dynamicstreamindex_p PI : P.getDynamicLoads()) {
    const Signal::SignalListTy Ports = getInSignals(PI);
    L.insert(std::end(L), std::begin(Ports), std::end(Ports));
  }

  // Stores
  for(staticstreamindex_p PI : P.getStaticStores()) {
    const Signal::SignalListTy Ports = getOutSignals(PI);
    L.insert(std::end(L), std::begin(Ports), std::end(Ports));
  }
  for(dynamicstreamindex_p PI : P.getDynamicStores()) {
    const Signal::SignalListTy Ports = getOutSignals(PI);
    L.insert(std::end(L), std::begin(Ports), std::end(Ports));
  }

  return L;
}

const Signal::SignalListTy oclacc::getOutSignals(const ScalarPort &P) {
  Signal::SignalListTy L;

  if (!P.isPipelined())
    llvm_unreachable("No non-pipelined OurScalars");

  unsigned BitWidth = P.getBitWidth();

  // No need to differentiate between Kernels and Blocks like in getInSignals()
  // as Kernels do not have scalar outputs.

  const std::string PName = getOpName(P);

  L.push_back(Signal(PName, BitWidth, Signal::Out, Signal::Reg));

  L.push_back(Signal(PName+"_valid", 1, Signal::Out, Signal::Reg));

  L.push_back(Signal(PName+"_ack", 1, Signal::In, Signal::Wire));

  return L;
}

const Signal::SignalListTy oclacc::getInSignals(const staticstreamindex_p P) {
  return getInSignals(*P);
}

const Signal::SignalListTy oclacc::getInSignals(const StaticStreamIndex &P) {
  Signal::SignalListTy L;

  // TODO: Store bitwidth for constant addresses
  unsigned AddressWidth = P.getBitWidth();
  unsigned DataWidth = P.getStream()->getBitWidth();

  const std::string PName = getOpName(P);

  L.push_back(Signal(PName+"_address", AddressWidth, Signal::Out, Signal::Reg));
  L.push_back(Signal(PName, DataWidth, Signal::In, Signal::Wire));
  L.push_back(Signal(PName+"_valid", 1, Signal::Out, Signal::Reg));
  L.push_back(Signal(PName+"_ack", 1, Signal::In, Signal::Wire));

  return L;
}

const Signal::SignalListTy oclacc::getInSignals(const dynamicstreamindex_p P) {
  return getInSignals(*P);
}

const Signal::SignalListTy oclacc::getInSignals(const DynamicStreamIndex &P) {
  Signal::SignalListTy L;

  unsigned AddressWidth = 64;
  unsigned DataWidth = P.getStream()->getBitWidth();

  const std::string Name = getOpName(P);

  L.push_back(Signal(Name+"_address", AddressWidth, Signal::Out, Signal::Reg));
  L.push_back(Signal(Name, DataWidth, Signal::In, Signal::Wire));
  L.push_back(Signal(Name+"_valid", 1, Signal::In, Signal::Wire));
  L.push_back(Signal(Name+"_ack", 1, Signal::Out, Signal::Reg));

  return L;
}


const Signal::SignalListTy oclacc::getOutSignals(const staticstreamindex_p P) {
  return getOutSignals(*P);
}

const Signal::SignalListTy oclacc::getOutSignals(const StaticStreamIndex &P) {
  Signal::SignalListTy L;

  const std::string PName = getOpName(P);

  // TODO Store bitwidth for constant addresses
  unsigned AddressWidth = 64;
  unsigned DataWidth = P.getStream()->getBitWidth();

  L.push_back(Signal(PName+"_address", AddressWidth, Signal::Out, Signal::Reg));
  L.push_back(Signal(PName, DataWidth, Signal::Out, Signal::Reg));
  L.push_back(Signal(PName+"_valid", 1, Signal::Out, Signal::Reg));
  L.push_back(Signal(PName+"_ack", 1, Signal::In, Signal::Wire));

  return L;
}

const Signal::SignalListTy oclacc::getOutSignals(const dynamicstreamindex_p P) {
  return getOutSignals(*P);
}

const Signal::SignalListTy oclacc::getOutSignals(const DynamicStreamIndex &P) {
  Signal::SignalListTy L;

  const std::string PName = getOpName(P);

  // Create pairs of address and data port for each store

  // Make sure that negativ array indices do not result in incollect signal
  // names

  const base_p Index = P.getIndex();

  unsigned AddressWidth = Index->getBitWidth();
  unsigned DataWidth = P.getStream()->getBitWidth();

  L.push_back(Signal(PName+"_address", AddressWidth, Signal::Out, Signal::Reg));
  L.push_back(Signal(PName, DataWidth, Signal::Out, Signal::Reg));
  L.push_back(Signal(PName+"_valid", 1, Signal::Out, Signal::Reg));
  L.push_back(Signal(PName+"_ack", 1, Signal::In, Signal::Wire));

  return L;
}

const std::string oclacc::createPortList(const Signal::SignalListTy &Ports) {
  std::stringstream S;

  std::string Prefix = "";

  for (const Signal &P : Ports) {
    S << Prefix << Indent(1) << P.getDefStr();

    Prefix = ",\n";
  }
  return S.str();
}



