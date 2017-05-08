#ifndef NAMING_H
#define NAMING_H


#include "Signal.h"
#include "HW/Port.h"
#include "../../HW/typedefs.h"


namespace oclacc {

class DesignUnit;

/// \brief Return the name used to connect a component to the component
/// generating the signal.
///
/// The name depends on its type:
/// - StreamIndices:
///   <stream>_<streamindex>_<sink_uid>
/// - ScanarInputs:
///   <name>_<uid_source>_<uid_sink>
///
const std::string getOpName(const base_p P);

const std::string getOpName(const HW &R);
const std::string getOpName(const ConstVal &R);
const std::string getOpName(const StreamAccess &R);

const Signal::SignalListTy getSignals(const DesignUnit &);

// Components

const Signal::SignalListTy getSignals(const Block &);
inline const Signal::SignalListTy getSignals(const block_p P) {
  return getSignals(*P);
}

const Signal::SignalListTy getSignals(const Kernel &);
inline const Signal::SignalListTy getSignals(const kernel_p P) {
  return getSignals(*P);
}


// Streams
const Signal::SignalListTy getSignals(const StreamPort &);
inline const Signal::SignalListTy getSignals(const streamport_p P) {
  return getSignals(*P);
}


const std::string createPortList(const Signal::SignalListTy &);

// Scalars

const Signal::SignalListTy getInSignals(const ScalarPort &);
inline const Signal::SignalListTy getInSignals(const scalarport_p P) {
  return getInSignals(*P);
}

const Signal::SignalListTy getInMuxSignals(const ScalarPort &, const ScalarPort &);
inline const Signal::SignalListTy getInMuxSignals(const scalarport_p Sink, const scalarport_p Src) {
  return getInMuxSignals(*Sink, *Src);
}

const Signal::SignalListTy getOutSignals(const ScalarPort &);
inline const Signal::SignalListTy getOutSignals(const scalarport_p P) {
  return getOutSignals(*P);
}

// Load and Store
const Signal::SignalListTy getSignals(const LoadAccess &);
inline const Signal::SignalListTy getSignals(const loadaccess_p P) {
  return getSignals(*P);
}

const Signal::SignalListTy getSignals(const StoreAccess &);
inline const Signal::SignalListTy getSignals(const storeaccess_p P) {
  return getSignals(*P);
}

// Used for delegation
inline const Signal::SignalListTy getSignals(const StreamAccess &R) {
  if (R.isLoad()) return getSignals(static_cast<const LoadAccess &>(R));
  else return getSignals(static_cast<const StoreAccess &>(R));
}

inline const Signal::SignalListTy getSignals(const streamaccess_p P) {
  return getSignals(*P);
}
} // end ns oclacc

#endif /* NAMING_H */
