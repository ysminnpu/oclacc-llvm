#ifndef DF_H
#define DF_H

#include "llvm/Support/Debug.h"

#include "Base.h"
#include "../typedefs.h"
#include "../HW.h"
#include "../Arith.h"
#include "../Control.h"
#include "../Streams.h"
#include "../Memory.h"
#include "../Constant.h"
#include "../Kernel.h"
#include "../Compare.h"

//#define TYPENAME(x) abi::__cxa_demangle(typeid(x).name(),0,0,NULL)
#define TYPENAME(x) x.getName()

namespace oclacc {

class DFVisitor : public BaseVisitor
{
  private:
    DFVisitor(const DFVisitor&) = delete;
    DFVisitor &operator =(const DFVisitor &) = delete;

  protected:
    DFVisitor() {};

  public:
    virtual int visit(BaseVisitable &r) {
      return 0;
    }

    virtual int visit(FPArith &r) {
      DEBUG_WITH_TYPE("DFVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      for (base_p p : r.getOuts()) {
        p->accept(*this);
      }
      return 0;
    }

    virtual int visit(Arith &r)
    {
      DEBUG_WITH_TYPE("DFVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      for (base_p p : r.getOuts()) {
        p->accept(*this);
      }
      return 0;
    }

    virtual int visit(Add &r)  { return visit(static_cast<Arith &>(r));}
    virtual int visit(FAdd &r) { return visit(static_cast<Arith &>(r));}
    virtual int visit(Sub &r)  { return visit(static_cast<Arith &>(r));}
    virtual int visit(FSub &r) { return visit(static_cast<FPArith &>(r));}
    virtual int visit(Mul &r)  { return visit(static_cast<Arith &>(r));}
    virtual int visit(FMul &r) { return visit(static_cast<FPArith &>(r));}
    virtual int visit(UDiv &r) { return visit(static_cast<Arith &>(r));}
    virtual int visit(SDiv &r) { return visit(static_cast<Arith &>(r));}
    virtual int visit(FDiv &r) { return visit(static_cast<FPArith &>(r));}
    virtual int visit(URem &r) { return visit(static_cast<Arith &>(r));}
    virtual int visit(SRem &r) { return visit(static_cast<Arith &>(r));}
    virtual int visit(FRem &r) { return visit(static_cast<FPArith &>(r));}
    virtual int visit(And &r)  { return visit(static_cast<Arith &>(r));}
    virtual int visit(Or &r)   { return visit(static_cast<Arith &>(r)); }

    virtual int visit(Compare &r)
    {
      DEBUG_WITH_TYPE("DFVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      for ( base_p p : r.getOuts() ) {
        p->accept(*this);
      }
      return 0;
    }

    //
    //Mux
    //
    virtual int visit(Mux &r)
    {
      DEBUG_WITH_TYPE("DFVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      for ( base_p p : r.getOuts() ) {
        p->accept(*this);
      }

      return 0;
    }

    //
    //Mem
    //
    virtual int visit(Reg &r)
    {
      DEBUG_WITH_TYPE("DFVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");
      
      for ( base_p p : r.getOuts() ) {
        p->accept(*this);
      }

      return 0;
    }
    virtual int visit(Ram &r)
    {
      DEBUG_WITH_TYPE("DFVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      r.index->accept(*this);

      for ( base_p p : r.getOuts() ) {
        p->accept(*this);
      }

      return 0;
    }
    virtual int visit(Fifo &r)
    {
      DEBUG_WITH_TYPE("DFVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      for ( base_p p : r.getOuts() ) {
        p->accept(*this);
      }

      return 0;
    }

    //
    //Constants
    //
    virtual int visit(ConstVal &r)
    {
      DEBUG_WITH_TYPE("DFVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");
      
      for ( base_p p : r.getOuts() ) {
        p->accept(*this);
      }
      return 0;
    }

    //
    //Design Unit
    //
    virtual int visit(DesignUnit &r)
    {
      DEBUG_WITH_TYPE("DFVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      for ( kernel_p p : r.Kernels ) {
        p->accept(*this);
      }

      return 0;
    }

    ///
    /// visit Kernel
    ///
    virtual int visit(Kernel &R)
    {
      DEBUG_WITH_TYPE("DFVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      for ( block_p P : R.getBlocks()) {
        P->accept(*this);
      }

#if 0
      for ( base_p P : R.getInScalars() ) {
        P->accept(*this);
      }
      for ( base_p P : R.getInStreams() ) {
        P->accept(*this);
      }

      for ( base_p P : R.getOutScalars() ) {
        P->accept(*this);
      }
      for ( base_p P : R.getOutStreams() ) {
        P->accept(*this);
      }

      for ( const_p P : R.getConstVals()) {
        P->accept(*this);
      }
#endif

      return 0;
    }

    virtual int visit(Block &R)
    {
      DEBUG_WITH_TYPE("DFVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      for ( base_p P : R.getIns() ) {
        P->accept(*this);
      }
      for ( base_p P : R.getOuts() ) {
        P->accept(*this);
      }
      for ( base_p P : R.getOps() ) {
        P->accept(*this);
      }

      return 0;
    }

    virtual int visit(InStream &r)  { return visit(static_cast<Stream &>(r)); }
    virtual int visit(OutStream &r) { return visit(static_cast<Stream &>(r)); }

#if 0
    virtual int visit(InStream &r) 
    {
      DEBUG_WITH_TYPE("DFVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      return 0;
    }

    virtual int visit(OutStream &r)
    {
      DEBUG_WITH_TYPE("DFVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      return 0;
    }
#endif

    virtual int visit(Stream &r)
    {
      DEBUG_WITH_TYPE("DFVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      for ( streamindex_p P : r.getIndices() ) {
        P->accept(*this);
      }
      return 0;
    }

    virtual int visit(StreamIndex &r)
    {
      DEBUG_WITH_TYPE("DFVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      r.getStream()->accept(*this);

      for (base_p P : r.getIns())
        P->accept(*this);

      for (base_p P : r.getOuts())
        P->accept(*this);

      return 0;
    }

    virtual int visit(DynamicStreamIndex &r)
    {
      DEBUG_WITH_TYPE("DFVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      r.getIndex()->accept(*this);
      r.getStream()->accept(*this);

      for (base_p P : r.getOuts())
        P->accept(*this);

      return 0;
    }

    virtual int visit(StaticStreamIndex &r)
    {
      DEBUG_WITH_TYPE("DFVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      r.getStream()->accept(*this);

      for (base_p P : r.getIns())
        P->accept(*this);

      for (base_p P : r.getOuts())
        P->accept(*this);

      return 0;
    }

    virtual int visit(InScalar &r)
    {
      DEBUG_WITH_TYPE("DFVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      for ( base_p p : r.getOuts() ) {
        p->accept(*this);
      }
      return 0;
    }

#if 0
    virtual int visit(OutStreamIndex &r)
    {
      DEBUG_WITH_TYPE("DFVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      if ( r.getIns() )
        r.getIns()->accept(*this);

      if ( r.getOuts() )
        r.getOuts()->accept(*this);

      if ( r.Idx )
        r.Idx->accept(*this);

      return 0;
    }
#endif

    virtual int visit(OutScalar &r)
    {
      DEBUG_WITH_TYPE("DFVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");


      if ( base_p I = r.getIn() )
        I->accept(*this);

      return 0;
    }

#if 0
    virtual int visit(InOutStream &r)
    {
      DEBUG_WITH_TYPE("DFVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      if ( r.getIns() )
        r.getIns()->accept(*this);

      for ( base_p p : r.getOuts() ) {
        p->accept(*this);
      }

      return 0;
    }
#endif

    virtual int visit(Tmp &r)
    {
      DEBUG_WITH_TYPE("DFVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      for ( base_p p : r.getOuts() ) {
        p->accept(*this);
      }

      return 0;
    }
};

} //ns oclacc

#undef TYPENAME

#endif /* DF_H */
