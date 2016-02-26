#ifndef DOT_H
#define DOT_H

#include <fstream>
#include <cxxabi.h>
#include <typeinfo>
#include <sstream>

#include "llvm/Support/Debug.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"

#include "DFVisitor.h"
#include "../Control.h"

//#define TYPENAME(x) abi::__cxa_demangle(typeid(x).name(),0,0,NULL)
#define TYPENAME(x) x.getName()

namespace oclacc {

class DotVisitor : public DFVisitor
{
  private:
    typedef DFVisitor super;

    std::stringstream RankInStream;
    std::stringstream RankOutStream;

    std::unique_ptr<llvm::raw_fd_ostream> F;

    DotVisitor(const DotVisitor &) = delete;
    DotVisitor &operator =(const  DotVisitor &V) = delete;

    std::string nodeOutStream(const std::string &Label) {
      return "[shape=invhouse,fillcolor=\"/set312/10\",style=filled,tailport=n,label=\"" + Label + "\"]";
    }

  public:
    DotVisitor() {
      DEBUG_WITH_TYPE("DotVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n" );
    }

    ~DotVisitor() {
      DEBUG_WITH_TYPE("DotVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n" );
    }


    //
    //VISIT METHODS
    //
    virtual int visit(FPArith &R)
    {
      VISIT_ONCE(R);

      DEBUG_WITH_TYPE("DotVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      (*F) << "n" << R.getUID() << " [shape=pentagon,fillcolor=\"/bugn9/9\",style=filled,label=\"" << R.getUniqueName() << "\"];" << "\n";
      super::visit(R);

      for ( base_p p : R.getOuts() ) {
        (*F) << "n" << R.getUID() << " -> " << "n" << p->getUID() << ";\n";
      }
      return 0;
    }

    virtual int visit(Arith &R)
    {
      VISIT_ONCE(R);

      DEBUG_WITH_TYPE("DotVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      (*F) << "n" << R.getUID() << " [shape=pentagon,fillcolor=\"/bugn9/6\",style=filled,label=\"" << R.getUniqueName() << "\"];" << "\n";
      super::visit(R);
      for ( base_p p : R.getOuts() ) {
        (*F) << "n" << R.getUID() << " -> " << "n" << p->getUID() << ";\n";
      }
      return 0;
    }

    virtual int visit(Compare &R)
    {
      VISIT_ONCE(R);

      DEBUG_WITH_TYPE("DotVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      (*F) << "n" << R.getUID() << " [shape=square,fillcolor=\"/bugn9/8\",style=filled,label=\"" << R.getUniqueName() << "\"];" << "\n";
      super::visit(R);
      for ( base_p p : R.getOuts() ) {
        (*F) << "n" << R.getUID() << " -> " << "n" << p->getUID() << "\n";
      }

      return 0;
    }

    virtual int visit(Mux &R)
    {
      VISIT_ONCE(R)
#if 0
      DEBUG_WITH_TYPE("DotVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      block_p ThisBlock = R.getBlock();

      std::stringstream MuxDecl;
      MuxDecl << "n" << R.getUID() << " [shape=record,fillcolor=\"/set312/5\",style=filled,tailport=n,label=\"{" << R.getUniqueName() << " | { ";

      for ( Mux::muxinput &In: R.getMuxIns() ) {
        base_p V = In.first;
        block_p B = In.second;

        MuxDecl << "{ <n" << V->getUID() << "> " << V->getName();

        base_p Cond = B->getCondition();
        ConditionFlag CondF = B->getConditionFlag(ThisBlock);

        if (CondF != COND_NONE)
          MuxDecl << "| " << Cond->getUniqueName() << ": " << ConditionFlagNames[CondF];

        MuxDecl << "} | ";

        errs() << "Mux " << R.getName() << ", from " << B->getName() << ": " << V->getName() << "\n";
      }

      MuxDecl << "} } \"];" << "\n";

      (*F) << MuxDecl.str();

      for ( base_p p : R.getOuts() ) {
        (*F) << "n" << R.getUID() << " -> " << "n" << p->getUID() << "\n";
      }
#endif

      super::visit(R);

      return 0;
    }



#if 0

      std::stringstream MuxDecl;

      MuxDecl << "n" << r.getUID() << " [shape=record,fillcolor=\"/set312/5\",style=filled,tailport=n,label=\"{" << r.getUniqueName();

      const std::list<Mux::mux_input> Ins = r.getCondIns();

      for (const Mux::mux_input &I : Ins) {
        base_p HWIn = I.first;

        (*F) << "n" << HWIn->getUID() << " -> " << "n" << r.getUID() << "\n";

        MuxDecl << "{";

        for (const Mux::cond_input &C : I.second) {
          base_p P = C.first;
          ConditionFlag Flag = C.second;

          (*F) << "n" << P->getUID() << " -> " << "n" << r.getUID() << "\n";

          MuxDecl << P->getUniqueName() << " |";
        }

        MuxDecl << "}|";
      }

      MuxDecl << "}\"];" << "\n";

      (*F) << MuxDecl.str();


      for ( base_p p : r.getOuts() ) {
        (*F) << "n" << r.getUID() << " -> " << "n" << p->getUID() << "\n";
      }
#endif

    virtual int visit(Reg &r)
    {
      VISIT_ONCE(r)
      DEBUG_WITH_TYPE("DotVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      (*F) << "n" << r.getUID() << " [shape=rect,fillcolor=\"/reds9/3\",style=filled,tailport=n,label=\"" << r.getUniqueName() << "\"];" << "\n";
      super::visit(r);

      for ( base_p p : r.getOuts() ) {
        (*F) << "n" << r.getUID() << " -> " << "n" << p->getUID() << ";\n";
      }

      return 0;
    }

    virtual int visit(Ram &r)
    {
      VISIT_ONCE(r)
      DEBUG_WITH_TYPE("DotVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      (*F) << "n" << r.getUID() << " [shape=box3d,fillcolor=\"/reds9/4\",style=filled,tailport=n,label=\"" << r.getUniqueName() << "\\n" << "W=" << r.getBitWidth() << " D="<< r.D << "\"];" << "\n";
      super::visit(r);

      for ( base_p p : r.getOuts() ) {
        (*F) << "n" << r.getUID() << " -> " << "n" << p->getUID() << ";\n";
      }

      return 0;
    }

    virtual int visit(Fifo &r)
    {
      VISIT_ONCE(r)
      DEBUG_WITH_TYPE("DotVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      (*F) << "n" << r.getUID() << " [shape=rect,fillcolor=\"/reds9/5\",style=filled,tailport=n,label=\"" << r.getUniqueName() << "\\nD=" << r.D << "\"];" << "\n";
      super::visit(r);

      for ( base_p p : r.getOuts() ) {
        (*F) << "n" << r.getUID() << " -> " << "n" << p->getUID() << ";\n";
      }

      return 0;
    }

    virtual int visit(ConstVal &r)
    {
      VISIT_ONCE(r)
      DEBUG_WITH_TYPE("DotVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      (*F) << "n" << r.getUID() << " [shape=oval,fillcolor=\"/set312/12\",style=filled,tailport=s,label=\"" << r.getName() << "\"];" << "\n";
      super::visit(r);

      for ( base_p p : r.getOuts() ) {
        (*F) << "n" << r.getUID() << " -> " << "n" << p->getUID() << "\n";
      }

      return 0;
    }

    virtual int visit(DesignUnit &r)
    {
      DEBUG_WITH_TYPE("DotVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");;

      super::visit(r);

      return 0;
    }

    virtual int visit(Block &r)
    {
      VISIT_ONCE(r);
      DEBUG_WITH_TYPE("DotVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");;

      (*F) << "subgraph cluster_" << "n" << r.getUID() << " {" << "\n";
      (*F) << "label = \"block_" << r.getName()  << "\";" << "\n";

      super::visit(r);

      (*F) << "{ rank=source; " << RankInStream.str() << " }\n";
      (*F) << "{ rank=sink; " << RankOutStream.str() << " }\n";

      (*F) << "}" << "\n";

      return 0;
    }

    virtual int visit(Kernel &r)
    {
      VISIT_ONCE(r);
      DEBUG_WITH_TYPE("DotVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      if (r.getName().empty())
        llvm_unreachable("Kernel Name Invalid");

      std::string FileName = r.getName()+".dot";

      DEBUG_WITH_TYPE("FileIO", llvm::dbgs() << "Open File "+ FileName << "\n" );

      std::error_code Err;
      F = std::unique_ptr<llvm::raw_fd_ostream>( new llvm::raw_fd_ostream( FileName, Err, llvm::sys::fs::F_RW | llvm::sys::fs::F_Text ));
      if (Err)
        llvm_unreachable("File could not be opened.");

      (*F) << "digraph G { " << "\n";

      RankInStream.str("");
      RankInStream.clear();

      RankOutStream.str("");
      RankOutStream.clear();

      (*F) << "subgraph cluster_" << "n" << r.getUID() << " {" << "\n";
      (*F) << "label = \"kernel_" << r.getName()  << "\";" << "\n";

      for (port_p P : r.getInScalars()) {
        (*F) << "n" << P->getUID() << " [shape=box,fillcolor=\"/set312/10\",style=filled,tailport=n,label=\"" << P->getUniqueName() << "\"];" << "\n";
        RankInStream << "n" << P->getUID() << " ";
      }
      for (port_p P : r.getInStreams()) {
        (*F) << "n" << P->getUID() << " [shape=house,fillcolor=\"/set312/10\",style=filled,tailport=n,label=\"" << P->getUniqueName() << "\"];" << "\n";
        RankInStream << "n" << P->getUID() << " ";
      }
      for (port_p P : r.getOutScalars()) {
        (*F) << "n" << P->getUID() << " [shape=box,fillcolor=\"/set312/10\",style=filled,tailport=n,label=\"" << P->getUniqueName() << "\"];" << "\n";
        RankOutStream << "n" << P->getUID() << " ";
      }
      for (port_p P : r.getOutStreams()) {
        (*F) << "n" << P->getUID() << " [shape=invhouse,fillcolor=\"/set312/10\",style=filled,tailport=n,label=\"" << P->getUniqueName() << "\"];" << "\n";
        RankOutStream << "n" << P->getUID() << " ";
      }

      super::visit(r);

      (*F) << "{ rank=source; " << RankInStream.str() << " }\n";
      (*F) << "{ rank=sink; " << RankOutStream.str() << " }\n";

      (*F) << "}" << "\n";

      (*F) << "}" << "\n";

      F->close();

      return 0;
    }

    /// \brief Create Node for StreamPort.
    ///
    /// Each StreamIndex points to that Node. By using invisible edges between
    /// separate Loads and Stores to the Port, they are hierarchically ordered
    /// in the graph.
    virtual int visit(StreamPort &r)
    {
      VISIT_ONCE(r)
      DEBUG_WITH_TYPE("DotVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");


      const StreamPort::IndexListType &IndexList = r.getIndexList();

      for (StreamPort::IndexListConstIt I=IndexList.begin(), E=IndexList.end(); I != E; I++) {
        if (I+1 != E) {
          (*F) << "n" << (*I)->getUID() << " -> " << "n" << (*(std::next(I,1)))->getUID() << " [style=invis];\n";
        }
      }

      (*F) << "n" << r.getUID() << " " << nodeOutStream(r.getUniqueName()) << ";\n";

      super::visit(r);

      return 0;
    }

    virtual int visit(DynamicStreamIndex &r)
    {
      VISIT_ONCE(r)
      DEBUG_WITH_TYPE("DotVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      base_p Index  = r.getIndex();
      streamport_p Stream = r.getStream();

      (*F) << "n" << r.getUID() << " [shape=rarrow,fillcolor=\"/set312/10\",style=filled,tailport=n,label=\"" << Stream->getUniqueName() << "[" << Index->getUniqueName() << "]" << "\"];" << "\n";

      for ( base_p p : r.getOuts() ) {
        (*F) << "n" << r.getUID() << " -> " << "n" << p->getUID() << ";\n";
      }

      (*F) << "n" << r.getUID() << " -> " << "n" << Stream->getUID() << ";\n";

      super::visit(r);

      return 0;
    }

    virtual int visit(StaticStreamIndex &r)
    {
      VISIT_ONCE(r)
      DEBUG_WITH_TYPE("DotVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      streamport_p Stream = r.getStream();

      (*F) << "n" << r.getUID() << " [shape=rarrow,fillcolor=\"/set312/10\",style=filled,tailport=n,label=\"" << Stream->getUniqueName() << "[" << r.getIndex() << "]" << "\"];" << "\n";

      (*F) << "n" << r.getUID() << " -> " << "n" << Stream->getUID() << ";\n";

      for ( base_p p : r.getOuts() ) {
        (*F) << "n" << r.getUID() << " -> " << "n" << p->getUID() << ";\n";
      }

      super::visit(r);

      return 0;
    }

    virtual int visit(ScalarPort &r)
    {
      VISIT_ONCE(r)
      DEBUG_WITH_TYPE("DotVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      for ( base_p p : r.getOuts() ) {
        (*F) << "n" << r.getUID() << " -> " << "n" << p->getUID() << ";\n";
      }

      super::visit(r);
      return 0;
    }


#if 0
    virtual int visit(OutStreamIndex &r)
    {
      VISIT_ONCE(r)
      DEBUG_WITH_TYPE("DotVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      (*F) << "n" << r.getUID() << " [shape=box,fillcolor=\"/set312/10\",style=filled,tailport=n,label=\"" << "Store[]" << "\"];" << "\n";

      if ( r.Idx )
        (*F) << "n" << r.Idx->UID << " -> " << "n" << r.getUID() << " [headlabel=\"Idx\"];" << "\n";

      if ( r.getOuts() )
        (*F) << "n" << r.getUID() << " -> " << "n" << r.getOuts()->UID << "\n";
      else
        llvm_unreachable( "no Output port" );


      super::visit(r);

      return 0;
    }
#endif

#if 0
    virtual int visit(InOutStream &r)
    {
      VISIT_ONCE(r)
      DEBUG_WITH_TYPE("DotVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      //Input
      (*F) << "n" << r.getUID() << "in [shape=invhouse,fillcolor=\"/set312/10\",style=filled,tailport=n,label=\"" << r.getUniqueName() << "\"];" << "\n";

      RankInStream << "n" << r.getUID() << "in ";

      super::visit(r);

      for ( base_p p : r.getOuts() ) {
        (*F) << "n" << r.getUID() << "in -> " << "n" << p->getUID() << "\n";
      }

      (*F) << "n" << r.getUID() << " [shape=house,fillcolor=\"/set312/10\",style=filled,tailport=n,label=\"" << r.getUniqueName() << "\"];" << "\n";

      RankOutStream << "n" << r.getUID() << " ";


      //if ( r.in )
      //  (*F) << r.in->UID << " -> " << "n" << r.getUID() << "\n";

      return 0;
    }
#endif

};

} // end namespace oclacc

#undef TYPENAME

#endif /* DOT_H */
