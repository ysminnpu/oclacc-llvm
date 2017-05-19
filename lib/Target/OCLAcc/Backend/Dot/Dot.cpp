#include "llvm/Support/raw_ostream.h"

#include "Dot.h"
#include "HW/Writeable.h"
#include "HW/typedefs.h"

#include "Utils.h"
#include "Macros.h"

#define DEBUG_TYPE "dot"

// Color definitions
#define C_SCALARPORT "\"/purples9/4\""
#define C_STREAMPORT "\"/purples9/5\""
#define C_MUX        "\"/accent8/3\""
#define C_ARITH      "\"/blues9/5\""
#define C_CONSTVAL   "\"/reds9/4\""
#define C_FPARITH    "\"/greens9/5\""
#define C_COMPARE    "\"/blues9/7\""
#define C_SYNCH      "\"/piyg9/2\""
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

  for (scalarport_p P : R.getInScalars()) {
    F() << "n" << P->getUID() << " [shape=box,fillcolor=" << C_SCALARPORT << ",style=filled,tailport=n,label=\"" << P->getUniqueName() << "\"];\n";
    RankInStream << "n" << P->getUID() << " ";
  }
  for (streamport_p P : R.getStreams()) {
    if (P->hasLoads()) {
      F() << "ld" << P->getUID() << " [shape=house,fillcolor=" << C_STREAMPORT << ",style=filled,tailport=n,label=\"" << P->getUniqueName() << "\"];\n";
      RankInStream << "ld" << P->getUID() << " ";
    }
    if (P->hasStores()) {
      F() << "st" << P->getUID() << " [shape=invhouse,fillcolor=" << C_STREAMPORT << ",style=filled,tailport=n,label=\"" << P->getUniqueName() << "\"];\n";
      RankOutStream << "st" << P->getUID() << " ";
    }
  }
  for (scalarport_p P : R.getOutScalars()) {
    F() << "n" << P->getUID() << " [shape=box,fillcolor=" << C_SCALARPORT << ",style=filled,tailport=n,label=\"" << P->getUniqueName() << "\"];\n";
    RankOutStream << "n" << P->getUID() << " ";
  }

  super::visit(R);

  F() << Connections.str();

  F() << "{ rank=same; " << RankInStream.str() << "}\n";
  F() << "{ rank=same; " << RankOutStream.str() << "}\n";

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

  std::stringstream Conds;
  std::stringstream NegConds;

  bool HasConds=false;
  for (const Block::CondTy &C : R.getConds()) {
    HasConds=true;
    Conds << " " << C.first->getUniqueName();
  }
  for (const Block::CondTy &C : R.getNegConds()) {
    HasConds=true;
    NegConds << " !" << C.first->getUniqueName();
  }

  if (HasConds)
    F() << "label = \"" << R.getUniqueName() << " when" << Conds.str() << NegConds.str() << "\";" << "\n";
  else
    F() << "label = \"" << R.getUniqueName() << "\";" << "\n";

  super::visit(R);

  std::stringstream RankInStream;
  std::stringstream RankOutStream;

  for (port_p P : R.getInScalars()) {
    F() << "n" << P->getUID() << " [shape=box,fillcolor=" << C_SCALARPORT << ",style=filled,tailport=n,label=\"" << P->getUniqueName() << "\"];" << "\n";
    RankInStream << "n" << P->getUID() << " ";
  }

  for (port_p P : R.getOutScalars()) {
    F() << "n" << P->getUID() << " [shape=box,fillcolor=" << C_SCALARPORT << ",style=filled,tailport=n,label=\"" << P->getUniqueName() << "\"];" << "\n";
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

  F() << "n" << R.getUID() << " [shape=invtrapezium,fillcolor=" << C_ARITH << ",style=filled,label=\"" << R.getUniqueName() << "\n" << R.getOp() << "\"];" << "\n";

  super::visit(R);

  for ( base_p p : R.getOuts() ) {
    Conn() << "n" << R.getUID() << " -> " << "n" << p->getUID() << " [color=" << C_ARITH << ",fontcolor=" << C_ARITH << ",label=" << R.getBitWidth() << "];\n";
  }
  return 0;
}

int Dot::visit(FPArith &R) {
  VISIT_ONCE(R);

  DEBUG(dbgs() << __PRETTY_FUNCTION__ << "\n");

  F() << "n" << R.getUID() << " [shape=pentagon,fillcolor=" << C_FPARITH << ",style=filled,label=\"" << R.getUniqueName() << "\n" << R.getOp() << "\"];" << "\n";

  super::visit(R);

  for ( base_p p : R.getOuts() ) {
    Conn() << "n" << R.getUID() << " -> " << "n" << p->getUID() << " [color=" << C_FPARITH << ",fontcolor=" << C_FPARITH << ",label=" << R.getBitWidth() << "];\n";
  }
  return 0;
}

int Dot::visit(IntCompare &R) {
  VISIT_ONCE(R);

  DEBUG(dbgs() << __PRETTY_FUNCTION__ << "\n");

  F() << "n" << R.getUID() << " [shape=rectangle,fillcolor=" << C_COMPARE << ",style=filled,label=\"" << R.getUniqueName() << "\n" << R.getOp() << "\"];\n";

  super::visit(R);

  for ( base_p p : R.getOuts() ) {
    Conn() << "n" << R.getUID() << " -> " << "n" << p->getUID() << " [color=" << C_COMPARE << ",fontcolor=" << C_COMPARE << ",label=" << R.getBitWidth() << "];\n";
  }

  return 0;
}
int Dot::visit(FPCompare &R) {
  VISIT_ONCE(R);

  DEBUG(dbgs() << __PRETTY_FUNCTION__ << "\n");

  F() << "n" << R.getUID() << " [shape=rectangle,fillcolor=" << C_COMPARE << ",style=filled,label=\"" << R.getUniqueName() << "\n" << R.getOp() << "\"];\n";

  super::visit(R);

  for ( base_p p : R.getOuts() ) {
    Conn() << "n" << R.getUID() << " -> " << "n" << p->getUID() << " [color=" << C_COMPARE << ",fontcolor=" << C_COMPARE << ",label=" << R.getBitWidth() << "];\n";
  }

  return 0;
}

int Dot::visit(ConstVal &R) {
  VISIT_ONCE(R)
  DEBUG(dbgs() << __PRETTY_FUNCTION__ << "\n");

  F() << "n" << R.getUID() << " [shape=oval,fillcolor=" << C_CONSTVAL << ",style=filled,tailport=s,label=\"" << R.getName() << "\"];\n";
  super::visit(R);

  for ( base_p P : R.getOuts() ) {
    // Do not print Node when used by a Mux
    if (!std::dynamic_pointer_cast<Mux>(P)) {
      Conn() << "n" << R.getUID() << " -> " << "n" << P->getUID() << " [color=" << C_CONSTVAL << ",fontcolor=" << C_CONSTVAL << ",fontcolor=" << C_CONSTVAL << ",label=" << R.getBitWidth() << "];\n";
    }
  }

  return 0;
}

/// \brief Create Node for StreamPort.
///
/// We do not dispatch to the Index objects but handle loads and stores
/// here.
///
int Dot::visit(StreamPort &R) {
  VISIT_ONCE(R)
  DEBUG(dbgs() << __PRETTY_FUNCTION__ << "\n");

  // By using invisible edges between separate Loads and Stores to the Port,
  // they are hierarchically ordered in the graph.

  // No range-based loop to call std::next()
  const StreamPort::AccessListTy &AL = R.getAccessList();
  for (StreamPort::AccessListTy::const_iterator I=AL.begin(), E=AL.end(); I != E; I++) {
    if (std::next(I) != E) {
      Conn() << "n" << (*I)->getUID() << " -> " << "n" << (*(std::next(I)))->getUID() << " [style=invis];\n";
    }
  }

  // We do not instantiate the Index Operations here as they must be placed in
  // their containing block.

  // Draw connection from Index to base Stream only for Stores. Loads make the
  // graph look polluted.
  for ( loadaccess_p I : R.getLoads() ) {
    Conn() << "ld" << R.getUID() << " -> " << "n" << I->getUID() << " [color=" << C_STREAMPORT << ",fontcolor=" << C_STREAMPORT << ",label=" << R.getBitWidth() << "];\n";
  }
  for ( storeaccess_p I : R.getStores() ) {
    Conn() << "n" << I->getUID() << " -> " << "st" << R.getUID() << " [color=" << C_STREAMPORT << ",fontcolor=" << C_STREAMPORT << ",label=" << R.getBitWidth() << "];\n";
  }

  return 0;
}

int Dot::visit(LoadAccess &R) {
  VISIT_ONCE(R);
  DEBUG(dbgs() << __PRETTY_FUNCTION__ << "\n");

  F() << "n" << R.getUID() << " [shape=larrow,fillcolor=" << C_STREAMPORT << ",style=filled,tailport=n,label=\""  << R.getUniqueName() << "\"];\n";

  super::visit(R);

  for ( base_p O : R.getOuts() ) {
    Conn() << "n" << R.getUID() << " -> " << "n" << O->getUID() << " [color=" << C_STREAMPORT << ",fontcolor=" << C_STREAMPORT << ",label=" << R.getBitWidth() << "];\n";
  }

  return 0;
}
int Dot::visit(StoreAccess &R) {
  VISIT_ONCE(R);
  DEBUG(dbgs() << __PRETTY_FUNCTION__ << "\n");

  F() << "n" << R.getUID() << " [shape=rarrow,fillcolor=" << C_STREAMPORT << ",style=filled,tailport=n,label=\""  << R.getUniqueName() << "\"];\n";

  super::visit(R);

  return 0;
}

int Dot::visit(StaticStreamIndex &R) {
  VISIT_ONCE(R);
  DEBUG(dbgs() << __PRETTY_FUNCTION__ << "\n");
  F() << "n" << R.getUID() << " [shape=box,fillcolor=" << C_STREAMPORT << ",style=filled,tailport=n,label=\""  << R.getUniqueName() << "\n" << " @ " << R.getStream()->getUniqueName() << "\"];\n";

  super::visit(R);

  for ( base_p O : R.getOuts() ) {
    Conn() << "n" << R.getUID() << " -> " << "n" << O->getUID() << " [color=" << C_STREAMPORT << ",fontcolor=" << C_STREAMPORT << ",label=" << R.getBitWidth() << "];\n";
  }

  return 0;
}

int Dot::visit(DynamicStreamIndex &R) {
  VISIT_ONCE(R);
  DEBUG(dbgs() << __PRETTY_FUNCTION__ << "\n");
  F() << "n" << R.getUID() << " [shape=box,fillcolor=" << C_STREAMPORT << ",style=filled,tailport=n,label=\""  << R.getUniqueName() << "\n" << R.getStream()->getUniqueName() << "\n@" << R.getIndex()->getUniqueName() << "\"];\n";

  super::visit(R);

  for ( base_p O : R.getOuts() ) {
    Conn() << "n" << R.getUID() << " -> " << "n" << O->getUID() << " [color=" << C_STREAMPORT << ",fontcolor=" << C_STREAMPORT << ",label=" << R.getBitWidth() << "];\n";
  }

  return 0;
}

int Dot::visit(ScalarPort &R) {
  VISIT_ONCE(R)
  DEBUG(dbgs() << __PRETTY_FUNCTION__ << "\n");

  for ( base_p P : R.getOuts() ) {
    // Special handling of Muxers because they use sub-labels to connect their
    // inputs. As Muxers are created from PHINodes, only ScalarInputs may be
    // used as Muxer imput.
    //
    if(!std::dynamic_pointer_cast<Mux>(P)) {
      Conn() << "n" << R.getUID() << " -> " << "n" << P->getUID() << " [color=" << C_SCALARPORT << ",fontcolor=" << C_SCALARPORT << ",label=" << R.getBitWidth() << "];\n";
    }
  }

  super::visit(R);
  return 0;
}

int Dot::visit(Mux &R) {
  VISIT_ONCE(R);

  std::stringstream SS;
  SS << "<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\">";
  SS << "<TR>";
  unsigned count=0;
  for (const Mux::MuxInputTy P : R.getIns()) {
    port_p Port = P.first;
    base_p Cond = P.second;

    SS << "<TD PORT=\"in" << count << "\">"<< Cond->getUniqueName() << "</TD>";
    Conn() << "n" << Port->getUID() << " -> " << "n" << R.getUID() << ":in" << count << " [color=" << C_SCALARPORT << ",fontcolor=" << C_SCALARPORT << ",label=" << Cond->getBitWidth() << "];\n";
    count++;
  }
  SS << "</TR>";
  SS << "<TR><TD COLSPAN=\"" << count << "\">" << R.getUniqueName() << "</TD></TR>";
  SS << "</TABLE>";

  //F() << "n" << R.getUID() << " [shape=invtrapezium,fillcolor=" << C_MUX << ",style=filled,tailport=s,label=\"" << R.getName() << "\"];" << "\n";
  F() << "n" << R.getUID() << " [shape=plaintext,fillcolor=" << C_MUX << ",style=filled,tailport=s,label=<" << SS.str() << ">];" << "\n";

  super::visit(R);

  for ( base_p P : R.getOuts() ) {
    Conn() << "n" << R.getUID() << " -> " << "n" << P->getUID() << " [color=" << C_MUX << ",fontcolor=" << C_MUX << ",label=" << R.getBitWidth() << "];\n";
  }

  return 0;
}

int Dot::visit(Barrier &R) {
  VISIT_ONCE(R);

  DEBUG(dbgs() << __PRETTY_FUNCTION__ << "\n");

  F() << "n" << R.getUID() << " [shape=rectangle,fillcolor=" << C_SYNCH << ",style=filled,label=\"" << R.getUniqueName() << "\"];\n";

  return 0;
}
