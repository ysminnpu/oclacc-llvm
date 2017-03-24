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
const std::string getOpName(const StaticStreamIndex &R);
const std::string getOpName(const DynamicStreamIndex &R);
//const std::string getOpName(const ScalarPort &R);

// Components
const PortListTy getSignals(const block_p);
const PortListTy getSignals(const Block &);

const PortListTy getSignals(const kernel_p);
const PortListTy getSignals(const Kernel &);

// Streams
const PortListTy getSignals(const streamport_p);
const PortListTy getSignals(const StreamPort &);

const std::string createPortList(const PortListTy &);

// Scalars
const PortListTy getInSignals(const scalarport_p);
const PortListTy getOutSignals(const scalarport_p);

const PortListTy getInSignals(const ScalarPort &);
const PortListTy getOutSignals(const ScalarPort &);

// Used for delegation
const PortListTy getInSignals(const streamindex_p);
const PortListTy getOutSignals(const streamindex_p);

const PortListTy getOutSignals(const StaticStreamIndex &);
const PortListTy getInSignals(const StaticStreamIndex &);

const PortListTy getInSignals(const DynamicStreamIndex &);
const PortListTy getOutSignals(const DynamicStreamIndex &);

} // end ns oclacc

#endif /* NAMING_H */
