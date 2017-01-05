#include "llvm/Support/raw_ostream.h"

#include "Dot.h"
#include "HW/Writeable.h"
#include "HW/typedefs.h"

#include "Utils.h"

#define DEBUG_TYPE "dot"

// Color definitions
#define C_SCALARPORT "\"/purples9/4\""
#define C_STREAMPORT "\"/purples9/5\""
#define C_MUX        "\"/accent8/4\""
#define C_ARITH      "\"/blues9/5\""
#define C_CONSTVAL   "\"/set312/12\""
#define C_FPARITH    "\"/greens9/5\""
#define C_COMPARE    "\"/blues9/7\""
using namespace oclacc;

Dot::Dot() : IndentLevel(0) {
  DEBUG(dbgs() << __PRETTY_FUNCTION__ << "\n");;
}

Dot::~Dot() {
  DEBUG(dbgs() << __PRETTY_FUNCTION__ << "\n");;
}

int Dot::visit(DesignUnit &R) {
  DEBUG(dbgs() << __PRETTY_FUNCTION__ << "\n");;

  super::visit(R);

  return 0;
}

int Dot::visit(Kernel &R) {
  VISIT_ONCE(R);
  DEBUG(dbgs() << __PRETTY_FUNCTION__ << "\n");

  if (R.getName().empty())
    llvm_unreachable("Kernel Name Invalid");

  const std::string FileName = R.getName()+".dot";

  DEBUG(llvm::dbgs() << "Open File "+ FileName << "\n" );

  FS = openFile(FileName);

  F() << "digraph G {\n";

  IndentLevel++;

  F() << "ranksep=0.8\n";
  F() << "nodesep=0.5\n";

  std::stringstream RankInStream;
  std::stringstream RankOutStream;

  F() << "subgraph cluster_" << "n" << R.getUID() << " {" << "\n";
  IndentLevel++;

  F() << "label = \"kernel_" << R.getName()  << "\";" << "\n";

  for (port_p P : R.getInScalars()) {
    F() << "n" << P->getUID() << " [shape=box,fillcolor=" << C_SCALARPORT << ",style=filled,tailport=n,label=\"" << P->getUniqueName() << "\"];\n";
    RankInStream << "n" << P->getUID() << " ";
  }
  for (port_p P : R.getInStreams()) {
    F() << "n" << P->getUID() << "[shape=house,fillcolor=" << C_STREAMPORT << ",style=filled,tailport=n,label=\"" << P->getUniqueName() << "\"];\n";
    RankInStream << "n" << P->getUID() << " ";
  }
  for (port_p P : R.getOutScalars()) {
    F() << "n" << P->getUID() << " [shape=box,fillcolor=" << C_SCALARPORT << ",style=filled,tailport=n,label=\"" << P->getUniqueName() << "\"];\n";
    RankOutStream << "n" << P->getUID() << " ";
  }
  for (port_p P : R.getOutStreams()) {
    F() << "n" << P->getUID() << "[shape=invhouse,fillcolor=" << C_STREAMPORT << ",style=filled,tailport=n,label=\"" << P->getUniqueName() << "\"];\n";
    RankOutStream << "n" << P->getUID() << " ";
  }

  super::visit(R);

  F() << Connections.str();

  F() << "{ rank=source; " << RankInStream.str() << "}\n";
  F() << "{ rank=sink; " << RankOutStream.str() << "}\n";

  IndentLevel--;
  F() << "}" << "\n";

  IndentLevel--;
  F() << "}" << "\n";

  FS->close();

  return 0;
}

int Dot::visit(Block &R) {
  VISIT_ONCE(R);
  DEBUG(dbgs() << __PRETTY_FUNCTION__ << "\n");;

  F() << "subgraph cluster_" << "n" << R.getUID() << " {" << "\n";
  IndentLevel++;

  F() << "label = \"block_" << R.getName()  << "\";" << "\n";

  super::visit(R);

  std::stringstream RankInStream;
  std::stringstream RankOutStream;

  for (port_p P : R.getInScalars()) {
    F() << "n" << P->getUID() << " [shape=box,fillcolor=" << C_SCALARPORT << ",style=filled,tailport=n,label=\"" << P->getUniqueName() << "\"];" << "\n";
    RankInStream << "n" << P->getUID() << " ";
  }
  for (port_p P : R.getInStreams()) {
    F() << "n" << P->getUID() << "[shape=house,fillcolor=" << C_STREAMPORT << ",style=filled,tailport=n,label=\"" << P->getUniqueName() << "\"];\n";
    RankInStream << "n" << P->getUID() << " ";
  }
  for (port_p P : R.getOutScalars()) {
    F() << "n" << P->getUID() << " [shape=box,fillcolor=" << C_SCALARPORT << ",style=filled,tailport=n,label=\"" << P->getUniqueName() << "\"];" << "\n";
    RankOutStream << "n" << P->getUID() << " ";
  }
  for (port_p P : R.getOutStreams()) {
    F() << "n" << P->getUID() << "[shape=invhouse,fillcolor=" << C_STREAMPORT << ",style=filled,tailport=n,label=\"" << P->getUniqueName() << "\"];\n";
    RankOutStream << "n" << P->getUID() << " ";
  }

  F() << "{ rank=source; " << RankInStream.str() << "}\n";
  F() << "{ rank=sink; " << RankOutStream.str() << "}\n";

  IndentLevel--;
  F() << "}" << "\n";

  return 0;
}

int Dot::visit(Arith &R) {
  VISIT_ONCE(R);

  DEBUG(dbgs() << __PRETTY_FUNCTION__ << "\n");

  F() << "n" << R.getUID() << " [shape=pentagon,fillcolor=" << C_ARITH << ",style=filled,label=\"" << R.getUniqueName() << "\"];" << "\n";

  super::visit(R);

  for ( base_p p : R.getOuts() ) {
    Conn() << "n" << R.getUID() << " -> " << "n" << p->getUID() << " [color=" << C_ARITH << ",fontcolor=" << C_ARITH << ",label=" << R.getBitWidth() << "];\n";
  }
  return 0;
}

int Dot::visit(FPArith &R) {
  VISIT_ONCE(R);

  DEBUG(dbgs() << __PRETTY_FUNCTION__ << "\n");

  F() << "n" << R.getUID() << " [shape=pentagon,fillcolor=" << C_FPARITH << ",style=filled,label=\"" << R.getUniqueName() << "\"];" << "\n";

  super::visit(R);

  for ( base_p p : R.getOuts() ) {
    Conn() << "n" << R.getUID() << " -> " << "n" << p->getUID() << " [color=" << C_FPARITH << ",fontcolor=" << C_FPARITH << ",label=" << R.getBitWidth() << "];\n";
  }
  return 0;
}

int Dot::visit(Compare &R) {
  VISIT_ONCE(R);

  DEBUG(dbgs() << __PRETTY_FUNCTION__ << "\n");

  F() << "n" << R.getUID() << " [shape=rectangle,fillcolor=" << C_COMPARE << ",style=filled,label=\"" << R.getUniqueName() << "\"];\n";

  super::visit(R);
  
  for ( base_p p : R.getOuts() ) {
    Conn() << "n" << R.getUID() << " -> " << "n" << p->getUID() << " [color=" << C_COMPARE << ",fontcolor=" << C_COMPARE << ",label=" << R.getBitWidth() << "];\n";
  }

  return 0;
}

#if 0 
int Dot::visit(Reg &r)
{
  VISIT_ONCE(r)
  DEBUG(dbgs() << __PRETTY_FUNCTION__ << "\n");

  F() << "n" << r.getUID() << " [shape=rect,fillcolor=\"/reds9/3\",style=filled,tailport=n,label=\"" << r.getUniqueName() << "\"];" << "\n";
  super::visit(r);

  for ( base_p p : r.getOuts() ) {
    F() << "n" << r.getUID() << " -> " << "n" << p->getUID() << ";\n";
  }

  return 0;
}

int Dot::visit(Ram &r)
{
  VISIT_ONCE(r)
  DEBUG(dbgs() << __PRETTY_FUNCTION__ << "\n");

  F() << "n" << r.getUID() << " [shape=box3d,fillcolor=\"/reds9/4\",style=filled,tailport=n,label=\"" << r.getUniqueName() << "\\n" << "W=" << r.getBitWidth() << " D="<< r.D << "\"];" << "\n";
  super::visit(r);

  for ( base_p p : r.getOuts() ) {
    F() << "n" << r.getUID() << " -> " << "n" << p->getUID() << ";\n";
  }

  return 0;
}

int Dot::visit(Fifo &r)
{
  VISIT_ONCE(r)
  DEBUG(dbgs() << __PRETTY_FUNCTION__ << "\n");

  F() << "n" << r.getUID() << " [shape=rect,fillcolor=\"/reds9/5\",style=filled,tailport=n,label=\"" << r.getUniqueName() << "\\nD=" << r.D << "\"];" << "\n";
  super::visit(r);

  for ( base_p p : r.getOuts() ) {
    F() << "n" << r.getUID() << " -> " << "n" << p->getUID() << ";\n";
  }

  return 0;
}
#endif

int Dot::visit(ConstVal &R) {
  VISIT_ONCE(R)
  DEBUG(dbgs() << __PRETTY_FUNCTION__ << "\n");

  F() << "n" << R.getUID() << " [shape=oval,fillcolor=" << C_CONSTVAL << ",style=filled,tailport=s,label=\"" << R.getName() << "\"];\n";
  super::visit(R);

  for ( base_p P : R.getOuts() ) {
    Conn() << "n" << R.getUID() << " -> " << "n" << P->getUID() << " [color=" << C_CONSTVAL << ",fontcolor=" << C_CONSTVAL << ",fontcolor=" << C_CONSTVAL << ",label=" << R.getBitWidth() << "];\n";
  }

  return 0;
}

/// \brief Create Node for StreamPort.
///
/// Each StreamIndex points to that Node. 
/// We do not dispatch to the Index objects but handle loads and stores
/// here.
///
int Dot::visit(StreamPort &R) {
  VISIT_ONCE(R)
  DEBUG(dbgs() << __PRETTY_FUNCTION__ << "\n");


  const StreamPort::IndexListTy &IndexList = R.getIndexList();

  // By using invisible edges between separate Loads and Stores to the Port,
  // they are hierarchically ordered in the graph. 
  //
  // No range-based loop to call std::next()
  for (StreamPort::IndexListConstIt I=IndexList.begin(), E=IndexList.end(); I != E; I++) {
    if (I+1 != E) {
      Conn() << "n" << (*I)->getUID() << " -> " << "n" << (*(std::next(I,1)))->getUID() << " [style=invis];\n";
    }
  }

  // We do not instantiate the Index Operations here as they must be placed in
  // their containing block.

#if 0
  // Separate loads/stores and static/dynamic indices to print arrows with
  // correct direction.
  for (streamindex_p I : R.getLoads()) {
    if (I->isStatic()) {
      staticstreamindex_p SI = std::static_pointer_cast<StaticStreamIndex>(I);
      F() << "n" << I->getUID() << nodeInStreamIndex(R.getUniqueName()+"["+std::to_string(SI->getIndex())+"]") << ";\n";
    } else {
      dynamicstreamindex_p DI = std::static_pointer_cast<DynamicStreamIndex>(I);
      F() << "n" << I->getUID() << nodeInStreamIndex(R.getUniqueName()+"["+DI->getUniqueName()+"]") << ";\n";
    B(streamindex_p I : R.getStores()) {
    if (I->isStatic()) {
      staticstreamindex_p SI = std::static_pointer_cast<StaticStreamIndex>(I);
      F() << "n" << I->getUID() << nodeOutStreamIndex(R.getUniqueName()+"["+std::to_string(SI->getIndex())+"]") << ";\n";
    } else {
      dynamicstreamindex_p DI = std::static_pointer_cast<DynamicStreamIndex>(I);
      F() << "n" << I->getUID() << nodeOutStreamIndex(R.getUniqueName()+"["+DI->getUniqueName()+"]") << ";\n";
    }
  }
#endif

  // Draw connection from Index to base Stream only for Stores. Loads make the
  // graph look polluted.
  for ( base_p I : R.getStores() ) {
    Conn() << "n" << I->getUID() << " -> " << "n" << R.getUID() << " [color=" << C_STREAMPORT << ",fontcolor=" << C_STREAMPORT << ",label=" << R.getBitWidth() << "];\n";
  }

#if 0
  for ( base_p I : R.getLoads() ) {
    Conn() << "n" << I->getUID() << " -> " << "n" << R.getUID() << " [color=" << C_STREAMPORT << ",fontcolor=" << C_STREAMPORT << "];\n";
  }
#endif

  // Draw connection from Index to uses
  // Stores do not have Outs().
  for ( base_p I : R.getLoads() ) {
    for ( base_p O : I->getOuts() ) {
      Conn() << "n" << I->getUID() << " -> " << "n" << O->getUID() << " [color=" << C_STREAMPORT << ",fontcolor=" << C_STREAMPORT << ",label=" << R.getBitWidth() << "];\n";
    }

  }

  return 0;
}

/// \brief Create arrow for Load/Store
///
int Dot::visit(StaticStreamIndex &R) {
  VISIT_ONCE(R)
  DEBUG(dbgs() << __PRETTY_FUNCTION__ << "\n");

  // Depending on being a Load or Store, the arrow direction must be correct.
  streamport_p HWStream = R.getStream();
  if (HWStream->isLoad(&R) )
    F() << "n" << R.getUID() << " [shape=larrow,fillcolor=" << C_STREAMPORT << ",style=filled,tailport=n,label=\""  << HWStream->getUniqueName() << "[" << R.getIndex() << "]\"];\n";
  else
    F() << "n" << R.getUID() << " [shape=rarrow,fillcolor=" << C_STREAMPORT << ",style=filled,tailport=n,label=\"" << HWStream->getUniqueName() << "[" << R.getIndex() << "]\"];\n";

  super::visit(R);

  return 0;
}

int Dot::visit(DynamicStreamIndex &R) {
  VISIT_ONCE(R)
  DEBUG(dbgs() << __PRETTY_FUNCTION__ << "\n");

  streamport_p HWStream = R.getStream();
  if (HWStream->isLoad(&R) )
    F() << "n" << R.getUID() << " [shape=larrow,fillcolor=" << C_STREAMPORT << ",style=filled,tailport=n,label=\"" << HWStream->getUniqueName() << "[" << R.getIndex()->getUniqueName() << "]\"];\n";
  else
    F() << "n" << R.getUID() << " [shape=rarrow,fillcolor=" << C_STREAMPORT << ",style=filled,tailport=n,label=\"" << HWStream->getUniqueName() << "[" << R.getIndex()->getUniqueName() << "]\"];\n";

  super::visit(R);

  return 0;
}

int Dot::visit(ScalarPort &R) {
  VISIT_ONCE(R)
  DEBUG(dbgs() << __PRETTY_FUNCTION__ << "\n");

  for ( base_p P : R.getOuts() ) {
    Conn() << "n" << R.getUID() << " -> " << "n" << P->getUID() << " [color=" << C_SCALARPORT << ",fontcolor=" << C_SCALARPORT << ",label=" << R.getBitWidth() << "];\n";
  }

  super::visit(R);
  return 0;
}

int Dot::visit(Mux &R) {
  VISIT_ONCE(R)

  F() << "n" << R.getUID() << " [shape=invtrapezium,fillcolor=" << C_MUX << ",style=filled,tailport=s,label=\"" << R.getName() << "\"];" << "\n";

  super::visit(R);

  for ( base_p P : R.getOuts() ) {
    Conn() << "n" << R.getUID() << " -> " << "n" << P->getUID() << " [color=" << C_MUX << ",fontcolor=" << C_MUX << ",label=" << R.getBitWidth() << "];\n";
  }

  return 0;
}
