#ifndef DF_H
#define DF_H

#include "llvm/Support/Debug.h"

#include "BaseVisitor.h"
#include "HW/typedefs.h"
#include "HW/HW.h"
#include "HW/Arith.h"
#include "HW/Control.h"
#include "HW/Streams.h"
#include "HW/Memory.h"
#include "HW/Constant.h"
#include "HW/Kernel.h"
#include "HW/Compare.h"
#include "HW/Design.h"
#include "HW/Kernel.h"

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
    virtual int visit(Visitable &R) {
      return 0;
    }

    virtual int visit(FPArith &R) {
      DEBUG_WITH_TYPE("DFVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      for (base_p p : R.getOuts()) {
        p->accept(*this);
      }
      return 0;
    }

    virtual int visit(Arith &R)
    {
      DEBUG_WITH_TYPE("DFVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      for (base_p p : R.getOuts()) {
        p->accept(*this);
      }
      return 0;
    }

    virtual int visit(Add &R)  { return visit(static_cast<Arith &>(R));}
    virtual int visit(Sub &R)  { return visit(static_cast<Arith &>(R));}
    virtual int visit(Mul &R)  { return visit(static_cast<Arith &>(R));}
    virtual int visit(UDiv &R) { return visit(static_cast<Arith &>(R));}
    virtual int visit(SDiv &R) { return visit(static_cast<Arith &>(R));}
    virtual int visit(URem &R) { return visit(static_cast<Arith &>(R));}
    virtual int visit(SRem &R) { return visit(static_cast<Arith &>(R));}

    virtual int visit(FAdd &R) { return visit(static_cast<FPArith &>(R));}
    virtual int visit(FSub &R) { return visit(static_cast<FPArith &>(R));}
    virtual int visit(FMul &R) { return visit(static_cast<FPArith &>(R));}
    virtual int visit(FDiv &R) { return visit(static_cast<FPArith &>(R));}
    virtual int visit(FRem &R) { return visit(static_cast<FPArith &>(R));}

    virtual int visit(Shl &R)  { return visit(static_cast<Arith &>(R));}
    virtual int visit(LShr &R) { return visit(static_cast<Arith &>(R)); }
    virtual int visit(AShr &R) { return visit(static_cast<Arith &>(R));}
    virtual int visit(And &R)  { return visit(static_cast<Arith &>(R)); }
    virtual int visit(Or &R)   { return visit(static_cast<Arith &>(R));}
    virtual int visit(Xor &R)  { return visit(static_cast<Arith &>(R)); }

    virtual int visit(Compare &R)
    {
      DEBUG_WITH_TYPE("DFVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      for ( base_p p : R.getOuts() ) {
        p->accept(*this);
      }
      return 0;
    }

    //
    //Mux
    //
    virtual int visit(Mux &R)
    {
      DEBUG_WITH_TYPE("DFVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      for ( base_p p : R.getOuts() ) {
        p->accept(*this);
      }

      return 0;
    }

    //
    //Mem
    //
    virtual int visit(Reg &R)
    {
      DEBUG_WITH_TYPE("DFVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");
      
      for ( base_p p : R.getOuts() ) {
        p->accept(*this);
      }

      return 0;
    }
    virtual int visit(Ram &R)
    {
      DEBUG_WITH_TYPE("DFVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      R.index->accept(*this);

      for ( base_p p : R.getOuts() ) {
        p->accept(*this);
      }

      return 0;
    }
    virtual int visit(Fifo &R)
    {
      DEBUG_WITH_TYPE("DFVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      for ( base_p p : R.getOuts() ) {
        p->accept(*this);
      }

      return 0;
    }

    //
    //Constants
    //
    virtual int visit(ConstVal &R)
    {
      DEBUG_WITH_TYPE("DFVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");
      
      for ( base_p p : R.getOuts() ) {
        p->accept(*this);
      }
      return 0;
    }

    //
    //Design Unit
    //
    virtual int visit(DesignUnit &R)
    {
      DEBUG_WITH_TYPE("DFVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      for ( kernel_p p : R.Kernels ) {
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

      for ( base_p P : R.getIns() ) {
        P->accept(*this);
      }
      for ( base_p P : R.getOuts() ) {
        P->accept(*this);
      }

      for ( const_p P : R.getConstVals()) {
        P->accept(*this);
      }

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


    virtual int visit(ScalarPort &R) { 
      DEBUG_WITH_TYPE("DFVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      //for (base_p P : R.getIns())
      //  P->accept(*this);

      //for (base_p P : R.getOuts())
      //  P->accept(*this);

      return 0;
    }

    virtual int visit(StreamPort &R)
    {
      DEBUG_WITH_TYPE("DFVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      for ( streamindex_p P : R.getIndexList() ) {
        P->accept(*this);
      }
      return 0;
    }

    virtual int visit(StreamIndex &R)
    {
      DEBUG_WITH_TYPE("DFVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      R.getStream()->accept(*this);

      //for (base_p P : R.getIns())
      //  P->accept(*this);

      //for (base_p P : R.getOuts())
      //  P->accept(*this);

      return 0;
    }

    virtual int visit(DynamicStreamIndex &R)
    {
      DEBUG_WITH_TYPE("DFVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      R.getIndex()->accept(*this);
      R.getStream()->accept(*this);

      for (base_p P : R.getOuts())
        P->accept(*this);

      return 0;
    }

    virtual int visit(StaticStreamIndex &R)
    {
      DEBUG_WITH_TYPE("DFVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      R.getStream()->accept(*this);

      for (base_p P : R.getIns())
        P->accept(*this);

      for (base_p P : R.getOuts())
        P->accept(*this);

      return 0;
    }
};

} //ns oclacc

#undef TYPENAME

#endif /* DF_H */
