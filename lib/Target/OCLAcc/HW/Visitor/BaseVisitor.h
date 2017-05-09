#ifndef BASEVISITOR_H
#define BASEVISITOR_H

#include <vector>

#include "llvm/Support/Debug.h"

#define VISIT_ONCE(x) \
  do { unsigned UID=x.getUID(); \
  if ( UID >= already_visited.size() ) {                     \
    already_visited.resize(already_visited.size() * 2, false); } \
  if ( already_visited[UID] ) return 0;                      \
  else already_visited[UID] = true; \
  } while (0);

#define VISIT_RESET(x) \
  do { unsigned UID=x.getUID(); \
  already_visited[UID] = false; \
  } while (0);

namespace oclacc {

class Visitable;

class DesignUnit;
class Kernel;
class Block;

class Arith;
class FPArith;
class Add;
class FAdd;
class Sub;
class FSub;
class Mul;
class FMul;
class UDiv;
class SDiv;
class FDiv;
class URem;
class SRem;
class FRem;

class Shl;
class LShr;
class AShr;
class And;
class Or;
class Xor;

class Compare;
class IntCompare;
class FPCompare;
class Mux;
class Reg;
class Ram;
class Fifo;
class ConstVal;

class Port;
class ScalarPort;
class StreamPort;
class StreamAccess;
class LoadAccess;
class StoreAccess;

class StreamIndex;
class DynamicStreamIndex;
class StaticStreamIndex;

class Barrier;

class BaseVisitor
{
  private:
    BaseVisitor(const BaseVisitor &V) = delete;
    BaseVisitor &operator =(const BaseVisitor &V) = delete;

  protected:
    BaseVisitor() : already_visited(1024, false) { }

  public:
    std::vector<bool> already_visited;


    virtual ~BaseVisitor() {
    };

    virtual int visit(Visitable &) = 0;
    virtual int visit(DesignUnit &) = 0;

    virtual int visit(Arith &) = 0;
    virtual int visit(FPArith &) = 0;
    virtual int visit(Add &) = 0;
    virtual int visit(FAdd &) = 0;
    virtual int visit(Sub & ) = 0;
    virtual int visit(FSub & ) = 0;
    virtual int visit(Mul & ) = 0;
    virtual int visit(FMul & ) = 0;
    virtual int visit(UDiv & ) = 0;
    virtual int visit(SDiv & ) = 0;
    virtual int visit(FDiv & ) = 0;
    virtual int visit(URem & ) = 0;
    virtual int visit(SRem & ) = 0;
    virtual int visit(FRem & ) = 0;

    virtual int visit(Shl & ) = 0;
    virtual int visit(LShr & )  = 0;
    virtual int visit(AShr & ) = 0;
    virtual int visit(And & )  = 0;
    virtual int visit(Or & ) = 0;
    virtual int visit(Xor & )  = 0;

    virtual int visit(Compare & ) = 0;
    virtual int visit(IntCompare & ) = 0;
    virtual int visit(FPCompare & ) = 0;
    virtual int visit(Mux & ) = 0;
    virtual int visit(Reg & ) = 0;
    virtual int visit(Ram & ) = 0;
    virtual int visit(Fifo & ) = 0;
    virtual int visit(ConstVal & ) = 0;
    virtual int visit(Kernel & ) = 0;
    virtual int visit(Block & ) = 0;

    virtual int visit(ScalarPort & )  = 0;
    virtual int visit(StreamPort & ) = 0;
    virtual int visit(StreamAccess &) = 0;
    virtual int visit(LoadAccess &) = 0;
    virtual int visit(StoreAccess &) = 0;

    virtual int visit(StreamIndex & ) = 0;
    virtual int visit(DynamicStreamIndex & ) = 0;
    virtual int visit(StaticStreamIndex & ) = 0;

    virtual int visit(Barrier & ) = 0;
};

} //ns oclacc

#endif /* BASEVISITOR_H */
