#ifndef VISITOR_H
#define VISITOR_H

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

class BaseVisitable;

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
class And;
class Or;
class Compare;
class Mux;
class Reg;
class Ram;
class Fifo;
class ConstVal;
//class InStreamIndex;
class InScalar;
//class OutStreamIndex;
class OutScalar;
//class InOutStream;
class Tmp;
class Stream;
class InStream;
class OutStream;
class StreamIndex;
class DynamicStreamIndex;
class StaticStreamIndex;

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

    virtual int visit( BaseVisitable &) = 0;
    virtual int visit( DesignUnit &) = 0;
    virtual int visit( Arith &) = 0;
    virtual int visit( FPArith &) = 0;
    virtual int visit( Add &) = 0;
    virtual int visit( FAdd &) = 0;
    virtual int visit( Sub & ) = 0;
    virtual int visit( FSub & ) = 0;
    virtual int visit( Mul & ) = 0;
    virtual int visit( FMul & ) = 0;
    virtual int visit( UDiv & ) = 0;
    virtual int visit( SDiv & ) = 0;
    virtual int visit( FDiv & ) = 0;
    virtual int visit( URem & ) = 0;
    virtual int visit( SRem & ) = 0;
    virtual int visit( FRem & ) = 0;
    virtual int visit( And & ) = 0;
    virtual int visit( Or & )  = 0;
    virtual int visit( Compare & ) = 0;
    virtual int visit( Mux & ) = 0;
    virtual int visit( Reg & ) = 0;
    virtual int visit( Ram & ) = 0;
    virtual int visit( Fifo & ) = 0;
    virtual int visit( ConstVal & ) = 0;
    virtual int visit( Kernel & ) = 0;
    virtual int visit( Block & ) = 0;
    //virtual int visit( InStreamIndex & ) = 0;
    virtual int visit( InScalar & )  = 0;
   // virtual int visit( OutStreamIndex & ) = 0;
    virtual int visit( OutScalar & ) = 0;
    //virtual int visit( InOutStream & ) = 0;
    virtual int visit( Stream & ) = 0;
    virtual int visit( InStream & )  = 0;
    virtual int visit( OutStream & ) = 0;
    virtual int visit( StreamIndex & ) = 0;
    virtual int visit( DynamicStreamIndex & ) = 0;
    virtual int visit( StaticStreamIndex & ) = 0;
    virtual int visit( Tmp & ) = 0;
};

} //ns oclacc

#endif /* VISITOR_H */
