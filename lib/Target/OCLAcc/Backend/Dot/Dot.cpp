#include "llvm/Support/raw_ostream.h"

#include "Dot.h"
#include "HW/Writeable.h"
#include "HW/typedefs.h"

#include "Utils.h"

#define DEBUG_TYPE "dot"

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

  F() << "digraph G { " << "\n";

  IndentLevel++;

  std::stringstream RankInStream;
  std::stringstream RankOutStream;

  F() << "subgraph cluster_" << "n" << R.getUID() << " {" << "\n";
  IndentLevel++;

  F() << "label = \"kernel_" << R.getName()  << "\";" << "\n";

  for (port_p P : R.getInScalars()) {
    F() << "n" << P->getUID() << " [shape=box,fillcolor=\"/purples9/4\",style=filled,tailport=n,label=\"" << P->getUniqueName() << "\"];" << "\n";
    RankInStream << "n" << P->getUID() << " ";
  }
  for (port_p P : R.getInStreams()) {
    F() << "n" << P->getUID() << nodeInStream(P->getUniqueName()) << ";\n";
    RankInStream << "n" << P->getUID() << " ";
  }
  for (port_p P : R.getOutScalars()) {
    F() << "n" << P->getUID() << " [shape=box,fillcolor=\"/purples9/4\",style=filled,tailport=n,label=\"" << P->getUniqueName() << "\"];" << "\n";
    RankOutStream << "n" << P->getUID() << " ";
  }
  for (port_p P : R.getOutStreams()) {
    F() << "n" << P->getUID() << nodeOutStream(P->getUniqueName()) << ";\n";
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
    F() << "n" << P->getUID() << " [shape=box,fillcolor=\"/purples9/4\",style=filled,tailport=n,label=\"" << P->getUniqueName() << "\"];" << "\n";
    RankInStream << "n" << P->getUID() << " ";
  }
  for (port_p P : R.getInStreams()) {
    F() << "n" << P->getUID() << nodeInStream(P->getUniqueName()) << ";\n";
  }
  for (port_p P : R.getOutScalars()) {
    F() << "n" << P->getUID() << " [shape=box,fillcolor=\"/purples9/4\",style=filled,tailport=n,label=\"" << P->getUniqueName() << "\"];" << "\n";
    RankOutStream << "n" << P->getUID() << " ";
  }
  for (port_p P : R.getOutStreams()) {
    F() << "n" << P->getUID() << nodeOutStream(P->getUniqueName()) << ";\n";
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

  F() << "n" << R.getUID() << " [shape=pentagon,fillcolor=\"/blues9/5\",style=filled,label=\"" << R.getUniqueName() << "\"];" << "\n";
  super::visit(R);
  for ( base_p p : R.getOuts() ) {
    Conn() << "n" << R.getUID() << " -> " << "n" << p->getUID() << ";\n";
  }
  return 0;
}

int Dot::visit(FPArith &R) {
  VISIT_ONCE(R);

  DEBUG(dbgs() << __PRETTY_FUNCTION__ << "\n");

  F() << "n" << R.getUID() << " [shape=pentagon,fillcolor=\"/greens9/5\",style=filled,label=\"" << R.getUniqueName() << "\"];" << "\n";
  super::visit(R);

  for ( base_p p : R.getOuts() ) {
    Conn() << "n" << R.getUID() << " -> " << "n" << p->getUID() << ";\n";
  }
  return 0;
}

int Dot::visit(Compare &R) {
  VISIT_ONCE(R);

  DEBUG(dbgs() << __PRETTY_FUNCTION__ << "\n");

  F() << "n" << R.getUID() << " [shape=rectangle,fillcolor=\"/blues9/7\",style=filled,label=\"" << R.getUniqueName() << "\"];" << "\n";
  super::visit(R);
  for ( base_p p : R.getOuts() ) {
    Conn() << "n" << R.getUID() << " -> " << "n" << p->getUID() << "\n";
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

int Dot::visit(ConstVal &r) {
  VISIT_ONCE(r)
  DEBUG(dbgs() << __PRETTY_FUNCTION__ << "\n");

  F() << "n" << r.getUID() << " [shape=oval,fillcolor=\"/set312/12\",style=filled,tailport=s,label=\"" << r.getName() << "\"];" << "\n";
  super::visit(r);

  for ( base_p p : r.getOuts() ) {
    Conn() << "n" << r.getUID() << " -> " << "n" << p->getUID() << "\n";
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

  // Separate loads/stores and static/dynamic indices to print arrows with
  // correct direction.
  for (streamindex_p I : R.getLoads()) {
    if (I->isStatic()) {
      staticstreamindex_p SI = std::static_pointer_cast<StaticStreamIndex>(I);
      F() << "n" << I->getUID() << nodeInStreamIndex(R.getUniqueName()+"["+std::to_string(SI->getIndex())+"]") << ";\n";
    } else {
      dynamicstreamindex_p DI = std::static_pointer_cast<DynamicStreamIndex>(I);
      F() << "n" << I->getUID() << nodeInStreamIndex(R.getUniqueName()+"["+DI->getUniqueName()+"]") << ";\n";
    }
  }
  for (streamindex_p I : R.getStores()) {
    if (I->isStatic()) {
      staticstreamindex_p SI = std::static_pointer_cast<StaticStreamIndex>(I);
      F() << "n" << I->getUID() << nodeOutStreamIndex(R.getUniqueName()+"["+std::to_string(SI->getIndex())+"]") << ";\n";
    } else {
      dynamicstreamindex_p DI = std::static_pointer_cast<DynamicStreamIndex>(I);
      F() << "n" << I->getUID() << nodeOutStreamIndex(R.getUniqueName()+"["+DI->getUniqueName()+"]") << ";\n";
    }
  }


  for ( base_p I : IndexList ) {
    // Draw connection from Index to base Stream
    // When the Stream is used with an Index, it must belong to a Parent Block.
    Conn() << "n" << I->getUID() << " -> " << "n" << R.getUID() << ";\n";

    // Draw connection from Index to uses
    // Stores do not have Outs().
    for ( base_p O : I->getOuts() ) {
      Conn() << "n" << I->getUID() << " -> " << "n" << O->getUID() << ";\n";
    }
  }


  return 0;
}

int Dot::visit(ScalarPort &R) {
  VISIT_ONCE(R)
  DEBUG(dbgs() << __PRETTY_FUNCTION__ << "\n");

  for ( base_p P : R.getOuts() ) {
    Conn() << "n" << R.getUID() << " -> " << "n" << P->getUID() << ";\n";
  }

  super::visit(R);
  return 0;
}

int Dot::visit(Mux &R) {
  VISIT_ONCE(R)

  F() << "n" << R.getUID() << " [shape=invtrapezium,fillcolor=\"/accent8/4\",style=filled,tailport=s,label=\"" << R.getName() << "\"];" << "\n";

  super::visit(R);

  for ( base_p P : R.getOuts() ) {
    Conn() << "n" << R.getUID() << " -> " << "n" << P->getUID() << ";\n";
  }

  return 0;
}
