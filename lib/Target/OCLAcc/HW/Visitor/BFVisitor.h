#ifndef BFVISITOR_H
#define BFVISITOR_H

#include <list>

#include "llvm/Support/Debug.h"

#include "BaseVisitor.h"
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
//#define TYPENAME(x) x.getUniqueName()

#define DEBUG_CALL(x) DEBUG_WITH_TYPE("BFVisitor", dbgs() << __PRETTY_FUNCTION__ << " " << x.getUniqueName() << "\n")

namespace oclacc {

enum VISIT_CODES {
  VISIT_OK,
  VISIT_AGAIN
};

class BFVisitor : public BaseVisitor
{
  private:
    std::list<base_p> ToVisit;

  protected:
    BFVisitor() {};

  public:
    BFVisitor(const BFVisitor&) = delete;
    BFVisitor &operator =(const BFVisitor &) = delete;

    void visitAll() {
      while (ToVisit.size()) {
        base_p P = ToVisit.front();
        ToVisit.pop_front();
        if (P->accept(*this) == VISIT_AGAIN) ToVisit.push_back(P);
      }
    }

    virtual int visit(Visitable &R) {
      return VISIT_OK;
    }

    virtual int visit(FPArith &R) {
      DEBUG_CALL(R);

      for (base_p P : R.getOuts()) {
        ToVisit.push_back(P);
      }
      return VISIT_OK;
    }

    virtual int visit(Arith &R)
    {
      DEBUG_CALL(R);

      for (base_p P : R.getOuts()) {
        ToVisit.push_back(P);
      }
      return VISIT_OK;
    }

    virtual int visit(Add &R)  { return visit(static_cast<Arith &>  (R));}
    virtual int visit(FAdd &R) { return visit(static_cast<Arith &>  (R));}
    virtual int visit(Sub &R)  { return visit(static_cast<Arith &>  (R));}
    virtual int visit(FSub &R) { return visit(static_cast<FPArith &>(R));}
    virtual int visit(Mul &R)  { return visit(static_cast<Arith &>  (R));}
    virtual int visit(FMul &R) { return visit(static_cast<FPArith &>(R));}
    virtual int visit(UDiv &R) { return visit(static_cast<Arith &>  (R));}
    virtual int visit(SDiv &R) { return visit(static_cast<Arith &>  (R));}
    virtual int visit(FDiv &R) { return visit(static_cast<FPArith &>(R));}
    virtual int visit(URem &R) { return visit(static_cast<Arith &>  (R));}
    virtual int visit(SRem &R) { return visit(static_cast<Arith &>  (R));}
    virtual int visit(FRem &R) { return visit(static_cast<FPArith &>(R));}

    virtual int visit(Shl &R)  { return visit(static_cast<Arith &>  (R));}
    virtual int visit(LShr &R)   { return visit(static_cast<Arith &>  (R)); }
    virtual int visit(AShr &R)  { return visit(static_cast<Arith &>  (R));}
    virtual int visit(And &R)   { return visit(static_cast<Arith &>  (R)); }
    virtual int visit(Or &R)  { return visit(static_cast<Arith &>  (R));}
    virtual int visit(Xor &R)   { return visit(static_cast<Arith &>  (R)); }

    virtual int visit(Compare &R)
    {
      DEBUG_CALL(R);

      for (base_p P : R.getOuts()) {
        ToVisit.push_back(P);
      }
      return VISIT_OK;
    }

    //
    //Mux
    //
    virtual int visit(Mux &R)
    {
      DEBUG_CALL(R);

      for ( base_p P : R.getOuts() ) {
        ToVisit.push_back(P);
      }

      return VISIT_OK;
    }

    //
    //Mem
    //
    virtual int visit(Reg &R)
    {
      DEBUG_CALL(R);

      for ( base_p P : R.getOuts() ) {
        ToVisit.push_back(P);
      }

      return VISIT_OK;
    }
    virtual int visit(Ram &R)
    {
      DEBUG_CALL(R);

      ToVisit.push_back(R.index);

      for ( base_p P : R.getOuts() ) {
        ToVisit.push_back(P);
      }

      return VISIT_OK;
    }
    virtual int visit(Fifo &R)
    {
      DEBUG_CALL(R);

      for ( base_p P : R.getOuts() ) {
        ToVisit.push_back(P);
      }

      return VISIT_OK;
    }

    //
    //Constants
    //
    virtual int visit(ConstVal &R)
    {
      DEBUG_CALL(R);

      for ( base_p P : R.getOuts() ) {
        ToVisit.push_back(P);
      }
      return VISIT_OK;
    }

    //
    //Design Unit
    //
    virtual int visit(DesignUnit &R)
    {
      DEBUG_CALL(R);

      for ( kernel_p P : R.Kernels ) {
        P->accept(*this);
      }

      return VISIT_OK;
    }

    virtual int visit(Block &R)
    {
      DEBUG_CALL(R);

      return VISIT_OK;
    }

    ///
    /// visit Kernel
    ///
    virtual int visit(Kernel &R)
    {
      DEBUG_CALL(R);

      for ( const_p P : R.getConstVals()) {
        ToVisit.push_back(P);
      }

      for ( base_p P : R.getInScalars() ) {
        ToVisit.push_back(P);
      }
      for ( streamport_p P : R.getInStreams() ) {
        for (streamindex_p SI : P->getIndices()) {
          ToVisit.push_back(SI);
        }
        ToVisit.push_back(P);
      }

      for ( base_p P : R.getOutScalars() ) {
        ToVisit.push_back(P);
      }
      for ( base_p P : R.getOutStreams() ) {
        ToVisit.push_back(P);
      }

      return VISIT_OK;
    }

#if 0
    virtual int visit(InStream &R) 
    {
      DEBUG_CALL(R);

      return VISIT_OK;
    }

    virtual int visit(OutStream &R)
    {
      DEBUG_CALL(R);

      return VISIT_OK;
    }
#endif

    virtual int visit(StreamPort &R)
    {
      DEBUG_CALL(R);

      for ( streamindex_p P : R.getIndices() ) {
        ToVisit.push_back(P);
      }
      return VISIT_OK;
    }

    virtual int visit(StreamIndex &R)
    {
      DEBUG_CALL(R);

      R.getStream()->accept(*this);

      for (base_p P : R.getIns())
        ToVisit.push_back(P);

      for (base_p P : R.getOuts())
        ToVisit.push_back(P);

      return VISIT_OK;
    }

    virtual int visit(DynamicStreamIndex &R)
    {
      DEBUG_CALL(R);

      R.getIndex()->accept(*this);
      R.getStream()->accept(*this);

#if 0
      for (base_p P : r.getInss())
        ToVisit.push_back(P);
#endif

      for (base_p P : R.getOuts())
        ToVisit.push_back(P);

      return VISIT_OK;
    }

    virtual int visit(StaticStreamIndex &R)
    {
      DEBUG_CALL(R);

      R.getStream()->accept(*this);

      for (base_p P : R.getIns())
        ToVisit.push_back(P);

      for (base_p P : R.getOuts())
        ToVisit.push_back(P);

      return VISIT_OK;
    }

#if 0
    virtual int visit(OutStreamIndex &R)
    {
      DEBUG_CALL(R);

      if ( R.In )
        R.In->accept(*this);

      if ( R.getOuts() )
        R.getOuts()->accept(*this);

      if ( R.Idx )
        R.Idx->accept(*this);

      return VISIT_OK;
    }
#endif

    virtual int visit(ScalarPort &R)
    {
      DEBUG_CALL(R);


      for ( base_p P : R.getIns() ) {
        ToVisit.push_back(P);
      }

      for ( base_p P : R.getOuts() ) {
        ToVisit.push_back(P);
      }

      return VISIT_OK;
    }

#if 0
    virtual int visit(InOutStream &R)
    {
      DEBUG_CALL(R);

      if ( R.In )
        R.In->accept(*this);

      for ( base_p P : R.getOuts() ) {
        ToVisit.push_back(P);
      }

      return VISIT_OK;
    }
#endif
};

} //ns oclacc

#undef DEBUG_CALL

#endif /* BFVISITOR_H */
