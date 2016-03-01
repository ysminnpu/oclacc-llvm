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
    virtual int visit(Visitable &r) {
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

    virtual int visit(Shl &r)  { return visit(static_cast<Arith &>(r));}
    virtual int visit(LShr &r)   { return visit(static_cast<Arith &>(r)); }
    virtual int visit(AShr &r)  { return visit(static_cast<Arith &>(r));}
    virtual int visit(And &r)   { return visit(static_cast<Arith &>(r)); }
    virtual int visit(Or &r)  { return visit(static_cast<Arith &>(r));}
    virtual int visit(Xor &r)   { return visit(static_cast<Arith &>(r)); }

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


    virtual int visit(ScalarPort &r) { 
      DEBUG_WITH_TYPE("DFVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      for (base_p P : r.getIns())
        P->accept(*this);

      for (base_p P : r.getOuts())
        P->accept(*this);

      return 0;
    }

    virtual int visit(StreamPort &r)
    {
      DEBUG_WITH_TYPE("DFVisitor", dbgs() << __PRETTY_FUNCTION__ << "\n");

      for ( streamindex_p P : r.getIndexList() ) {
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
};

} //ns oclacc

#undef TYPENAME

#endif /* DF_H */
