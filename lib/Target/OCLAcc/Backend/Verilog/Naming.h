#ifndef NAMING_H
#define NAMING_H


#include "Signal.h"
#include "../../HW/typedefs.h"


namespace oclacc {

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
const std::string getOpName(const ConstVal &P);
const std::string getOpName(const StaticStreamIndex &R);
const std::string getOpName(const DynamicStreamIndex &R);
//const std::string getOpName(const ScalarPort &R);

// Components
const Signal::SignalListTy getSignals(const block_p);
const Signal::SignalListTy getSignals(const Block &);

const Signal::SignalListTy getSignals(const kernel_p);
const Signal::SignalListTy getSignals(const Kernel &);

// Streams
const Signal::SignalListTy getSignals(const streamport_p);
const Signal::SignalListTy getSignals(const StreamPort &);

const std::string createPortList(const Signal::SignalListTy &);

// Scalars

const Signal::SignalListTy getInSignals(const scalarport_p);
const Signal::SignalListTy getInSignals(const ScalarPort &);

const Signal::SignalListTy getOutSignals(const scalarport_p);
const Signal::SignalListTy getOutSignals(const ScalarPort &);

// Used for delegation
const Signal::SignalListTy getInSignals(const streamindex_p);
const Signal::SignalListTy getOutSignals(const streamindex_p);

const Signal::SignalListTy getInSignals(const staticstreamindex_p);
const Signal::SignalListTy getInSignals(const StaticStreamIndex &);

const Signal::SignalListTy getOutSignals(const staticstreamindex_p);
const Signal::SignalListTy getOutSignals(const StaticStreamIndex &);

const Signal::SignalListTy getInSignals(const dynamicstreamindex_p);
const Signal::SignalListTy getInSignals(const DynamicStreamIndex &);

const Signal::SignalListTy getOutSignals(const dynamicstreamindex_p);
const Signal::SignalListTy getOutSignals(const DynamicStreamIndex &);

} // end ns oclacc

#endif /* NAMING_H */
