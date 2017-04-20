#ifndef DF_H
#define DF_H

#include "llvm/Support/Debug.h"

#include "BaseVisitor.h"
#include "Macros.h"
#include "HW/typedefs.h"
#include "HW/HW.h"
#include "HW/Arith.h"
#include "HW/Control.h"
#include "HW/Port.h"
#include "HW/Memory.h"
#include "HW/Constant.h"
#include "HW/Kernel.h"
#include "HW/Compare.h"
#include "HW/Design.h"
#include "HW/Kernel.h"

//#define TYPENAME(x) abi::__cxa_demangle(typeid(x).name(),0,0,NULL)
#define TYPENAME(x) x.getName()
#define DEBUG_TYPE "dfvisitor"

namespace oclacc {

class DFVisitor : public BaseVisitor
{
  private:
    DFVisitor(const DFVisitor&) = delete;
    DFVisitor &operator =(const DFVisitor &) = delete;

  protected:
    DFVisitor() {};

  public:
    virtual int visit(Visitable &R) override {
      return 0;
    }

    virtual int visit(FPArith &R) override {
      DEBUG_FUNC;

      for (base_p p : R.getOuts()) {
        p->accept(*this);
      }
      return 0;
    }

    virtual int visit(Arith &R) override {
      DEBUG_FUNC;

      for (base_p p : R.getOuts()) {
        p->accept(*this);
      }
      return 0;
    }

    virtual int visit(Add &R) override { return visit(static_cast<Arith &>(R));}
    virtual int visit(Sub &R) override { return visit(static_cast<Arith &>(R));}
    virtual int visit(Mul &R) override { return visit(static_cast<Arith &>(R));}
    virtual int visit(UDiv &R) override { return visit(static_cast<Arith &>(R));}
    virtual int visit(SDiv &R) override { return visit(static_cast<Arith &>(R));}
    virtual int visit(URem &R) override { return visit(static_cast<Arith &>(R));}
    virtual int visit(SRem &R) override { return visit(static_cast<Arith &>(R));}

    virtual int visit(FAdd &R) override { return visit(static_cast<FPArith &>(R));}
    virtual int visit(FSub &R) override { return visit(static_cast<FPArith &>(R));}
    virtual int visit(FMul &R) override { return visit(static_cast<FPArith &>(R));}
    virtual int visit(FDiv &R) override { return visit(static_cast<FPArith &>(R));}
    virtual int visit(FRem &R) override { return visit(static_cast<FPArith &>(R));}

    virtual int visit(Shl &R) override { return visit(static_cast<Arith &>(R));}
    virtual int visit(LShr &R) override { return visit(static_cast<Arith &>(R)); }
    virtual int visit(AShr &R) override { return visit(static_cast<Arith &>(R));}
    virtual int visit(And &R) override { return visit(static_cast<Arith &>(R)); }
    virtual int visit(Or &R) override { return visit(static_cast<Arith &>(R));}
    virtual int visit(Xor &R) override { return visit(static_cast<Arith &>(R)); }

    virtual int visit(Compare &R) override {
      DEBUG_FUNC;

      for ( base_p p : R.getOuts() ) {
        p->accept(*this);
      }
      return 0;
    }
    virtual int visit(IntCompare &R) override {
      DEBUG_FUNC;

      for ( base_p p : R.getOuts() ) {
        p->accept(*this);
      }
      return 0;
    }
    virtual int visit(FPCompare &R) override {
      DEBUG_FUNC;

      for ( base_p p : R.getOuts() ) {
        p->accept(*this);
      }
      return 0;
    }

    //
    //Mux
    //
    virtual int visit(Mux &R) override {
      DEBUG_FUNC;

      for ( base_p p : R.getOuts() ) {
        p->accept(*this);
      }

      return 0;
    }

    //
    //Mem
    //
    virtual int visit(Reg &R) override {
      DEBUG_FUNC;
      
      for ( base_p p : R.getOuts() ) {
        p->accept(*this);
      }

      return 0;
    }
    virtual int visit(Ram &R) override {
      DEBUG_FUNC;

      R.index->accept(*this);

      for ( base_p p : R.getOuts() ) {
        p->accept(*this);
      }

      return 0;
    }
    virtual int visit(Fifo &R) override {
      DEBUG_FUNC;

      for ( base_p p : R.getOuts() ) {
        p->accept(*this);
      }

      return 0;
    }

    //
    //Constants
    //
    virtual int visit(ConstVal &R) override {
      DEBUG_FUNC;
      
      for ( base_p p : R.getOuts() ) {
        p->accept(*this);
      }
      return 0;
    }

    //
    //Design Unit
    //
    virtual int visit(DesignUnit &R) override {
      DEBUG_FUNC;

      for ( kernel_p p : R.Kernels ) {
        p->accept(*this);
      }

      return 0;
    }

    ///
    /// visit Kernel
    ///
    virtual int visit(Kernel &R) override {
      DEBUG_FUNC;

      for ( block_p P : R.getBlocks()) {
        P->accept(*this);
      }

      for ( port_p P : R.getIns() ) {
        P->accept(*this);
      }
      for ( port_p P : R.getOuts() ) {
        P->accept(*this);
      }

      for ( const_p P : R.getConstVals()) {
        P->accept(*this);
      }

      return 0;
    }

    virtual int visit(Block &R) override {
      DEBUG_FUNC;

      for ( port_p P : R.getInScalars() ) {
        P->accept(*this);
      }
      for ( port_p P : R.getOutScalars() ) {
        P->accept(*this);
      }
      for ( base_p P : R.getOps() ) {
        P->accept(*this);
      }

      for ( const_p P : R.getConstVals()) {
        P->accept(*this);
      }

      return 0;
    }


    virtual int visit(ScalarPort &R) override { 
      DEBUG_FUNC;

      return 0;
    }

    virtual int visit(StreamPort &R) override {
      DEBUG_FUNC;

      return 0;
    }

    virtual int visit(StreamAccess &R) override {
      DEBUG_FUNC;

      return 0;
    }

    virtual int visit(LoadAccess &R) override {
      DEBUG_FUNC;

      return 0;
    }

    virtual int visit(StoreAccess &R) override {
      DEBUG_FUNC;
      
      return 0;
    }

    virtual int visit(StreamIndex &R) override {
      DEBUG_FUNC;

      R.getStream()->accept(*this);

      for (base_p P : R.getOuts())
        P->accept(*this);

      return 0;
    }

    virtual int visit(DynamicStreamIndex &R) override {
      DEBUG_FUNC;

      return visit(static_cast<StreamIndex &>(R));
    }

    virtual int visit(StaticStreamIndex &R) override {
      DEBUG_FUNC;

      return visit(static_cast<StreamIndex &>(R));
    }
};

} //ns oclacc

#undef TYPENAME
#ifdef DEBUG_TYPE
#undef DEBUG_TYPE
#endif

#ifdef DEBUG_FUNC
#undef DEBUG_FUNC
#endif


#endif /* DF_H */
