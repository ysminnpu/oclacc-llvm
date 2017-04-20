#include <sstream>

#include "llvm/Support/ErrorHandling.h"

#include "HW/Kernel.h"
#include "HW/Arith.h"
#include "HW/Constant.h"
#include "Naming.h"
#include "VerilogMacros.h"

using namespace oclacc;

extern Signal Clk;
extern Signal Rst;

const std::string oclacc::getOpName(const base_p P) {
  if (const_p SI = std::dynamic_pointer_cast<ConstVal>(P)) {
    return getOpName(*SI);
  } else if (streamaccess_p SA = std::dynamic_pointer_cast<StreamAccess>(P)) {
    return getOpName(*SA);
  } else {
    return getOpName(*P);
  }
}

const std::string oclacc::getOpName(const ConstVal &R) {
  std::stringstream Name;

  std::string PName = R.getUniqueName();
  if (PName[0] == '-')
    PName[0] = '_';

  Name << "const_" << PName;

  return Name.str();
}

const std::string oclacc::getOpName(const StreamAccess &R) {
  std::stringstream Name;

  const streamindex_p I = R.getIndex();
  const streamport_p S = I->getStream();
  
  Name << S->getName() << "_" << I->getUniqueName() << "_" << R.getUniqueName();

  return Name.str();
}

#if 0
const std::string oclacc::getOpName(const StaticStreamIndex &R) {
  std::stringstream Name;

  const std::string PName = R.getUniqueName();
  const std::string StreamName = R.getStream()->getName();

  // Get numerical index
  StaticStreamIndex::IndexTy Index = R.getIndex();
  std::string Sign = "";
  if (Index < 0) {
    Index = -Index;
    Sign = "_";
  }

  Name << StreamName << "_" << PName << "_" << "const" << Sign << Index;

  return Name.str();
}

const std::string oclacc::getOpName(const DynamicStreamIndex &R) {
  std::stringstream Name;

  const std::string IndexName = R.getUniqueName();
  const std::string StreamName = R.getStream()->getName();

  const base_p Index = R.getIndex();
  const std::string ID = std::to_string(Index->getUID());

  Name << StreamName+"_"+IndexName+"_"+ID;

  return Name.str();
}
#endif

const std::string oclacc::getOpName(const HW &R) {
  return R.getUniqueName();
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
  for (const loadaccess_p P : R.getLoads()) {
    const Signal::SignalListTy SIST = getSignals(P);
    L.insert(std::end(L),std::begin(SIST), std::end(SIST)); 
  }

  // Outouts
  for (const scalarport_p P : R.getOutScalars()) {
    const Signal::SignalListTy SOSC = getOutSignals(P);
    L.insert(std::end(L),std::begin(SOSC), std::end(SOSC)); 
  }

  for (const storeaccess_p P : R.getStores()) {
    const Signal::SignalListTy SOST = getSignals(P);
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

#if 0
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
#endif


/// \brief Return a list of all ScalarPorts
const Signal::SignalListTy oclacc::getInSignals(const ScalarPort &P) {
  Signal::SignalListTy L;

  unsigned BitWidth = P.getBitWidth();

  const std::string PName = getOpName(P);

  // Only pipelined ports have buffer and synchronization signals
  if (P.isPipelined()) {
    L.push_back(Signal(PName+"_unbuf",BitWidth,Signal::In, Signal::Wire));
    L.push_back(Signal(PName+"_unbuf_valid", 1, Signal::In, Signal::Wire));
    L.push_back(Signal(PName+"_ack", 1, Signal::Out, Signal::Reg));
  } else
    L.push_back(Signal(PName,BitWidth,Signal::In, Signal::Wire));

  return L;
}


const Signal::SignalListTy oclacc::getInMuxSignals(const ScalarPort &Sink, const ScalarPort &Src) {
  Signal::SignalListTy L;

  assert(Sink.isPipelined());
  assert(Src.isPipelined());

  unsigned BitWidth = Sink.getBitWidth();
  const std::string SrcName = getOpName(Src);
  const std::string SinkName = getOpName(Sink);

  // Only logic, no registers
  L.push_back(Signal(SinkName+"_"+SrcName, BitWidth, Signal::In, Signal::Wire));
  L.push_back(Signal(SinkName+"_"+SrcName+"_valid", 1, Signal::In, Signal::Wire));
  L.push_back(Signal(SinkName+"_"+SrcName+"_ack", BitWidth, Signal::Out, Signal::Wire));

  return L;
}

const Signal::SignalListTy oclacc::getSignals(const streamport_p P) {
  return getSignals(*P);
}

const Signal::SignalListTy oclacc::getSignals(const StreamPort &P) {
  Signal::SignalListTy L;

  const std::string PName = getOpName(P);

  for (streamaccess_p S : P.getAccessList()) {
    const Signal::SignalListTy Ports = getSignals(S);
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

  L.push_back(Signal(PName+"", BitWidth, Signal::Out, Signal::Reg));

  L.push_back(Signal(PName+"_valid", 1, Signal::Out, Signal::Reg));

  L.push_back(Signal(PName+"_ack", 1, Signal::In, Signal::Wire));

  return L;
}

#if 0

const Signal::SignalListTy oclacc::getInSignals(const staticstreamindex_p P) {
  return getInSignals(*P);
}

const Signal::SignalListTy oclacc::getInSignals(const StaticStreamIndex &P) {
  Signal::SignalListTy L;

  // TODO: Load bitwidth for constant addresses
  unsigned AddressWidth = P.getBitWidth();
  unsigned DataWidth = P.getStream()->getBitWidth();

  const std::string PName = getOpName(P);

  L.push_back(Signal(PName+"_address", AddressWidth, Signal::Out, Signal::Reg));
  L.push_back(Signal(PName+"_address_valid", 1, Signal::Out, Signal::Reg));
  L.push_back(Signal(PName+"_unbuf", DataWidth, Signal::In, Signal::Wire));
  L.push_back(Signal(PName+"_unbuf_valid", 1, Signal::In, Signal::Wire));
  L.push_back(Signal(PName+"_ack", 1, Signal::Out, Signal::Reg));

  return L;
}

const Signal::SignalListTy oclacc::getInSignals(const dynamicstreamindex_p P) {
  return getInSignals(*P);
}

const Signal::SignalListTy oclacc::getInSignals(const DynamicStreamIndex &P) {
  Signal::SignalListTy L;

  unsigned AddressWidth = 64;
  unsigned DataWidth = P.getStream()->getBitWidth();

  const std::string PName = getOpName(P);

  L.push_back(Signal(PName+"_address", AddressWidth, Signal::Out, Signal::Reg));
  L.push_back(Signal(PName+"_address_valid", 1, Signal::Out, Signal::Reg));
  L.push_back(Signal(PName+"_unbuf", DataWidth, Signal::In, Signal::Wire));
  L.push_back(Signal(PName+"_unbuf_valid", 1, Signal::In, Signal::Wire));
  L.push_back(Signal(PName+"_ack", 1, Signal::Out, Signal::Reg));

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
  L.push_back(Signal(PName+"_buf", DataWidth, Signal::Out, Signal::Reg));
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
  L.push_back(Signal(PName+"_buf", DataWidth, Signal::Out, Signal::Reg));
  L.push_back(Signal(PName+"_valid", 1, Signal::Out, Signal::Reg));
  L.push_back(Signal(PName+"_ack", 1, Signal::In, Signal::Wire));

  return L;
}
#endif
const Signal::SignalListTy oclacc::getSignals(const LoadAccess &R) {
  Signal::SignalListTy L;

  unsigned AddressWidth = 64;
  unsigned DataWidth = R.getStream()->getBitWidth();

  const std::string PName = getOpName(R);

  L.push_back(Signal(PName+"_address", AddressWidth, Signal::Out, Signal::Reg));
  L.push_back(Signal(PName+"_address_valid", 1, Signal::Out, Signal::Reg));
  L.push_back(Signal(PName+"_unbuf", DataWidth, Signal::In, Signal::Wire));
  L.push_back(Signal(PName+"_unbuf_valid", 1, Signal::In, Signal::Wire));
  L.push_back(Signal(PName+"_ack", 1, Signal::Out, Signal::Reg));

  return L;
}

const Signal::SignalListTy oclacc::getSignals(const StoreAccess &R) {
  Signal::SignalListTy L;

  const std::string PName = getOpName(R);

  const base_p Index = R.getIndex();

  unsigned AddressWidth = Index->getBitWidth();
  unsigned DataWidth = R.getStream()->getBitWidth();

  L.push_back(Signal(PName+"_address", AddressWidth, Signal::Out, Signal::Reg));
  L.push_back(Signal(PName+"_buf", DataWidth, Signal::Out, Signal::Reg));
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



