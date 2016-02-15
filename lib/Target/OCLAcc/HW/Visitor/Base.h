#ifndef BASE_H
#define BASE_H

#include "llvm/Support/Debug.h"

#include "Visitor.h"

//DEBUG_WITH_TYPE("Visitor", llvm::dbgs() << __PRETTY_FUNCTION__ << "\n" );

#define DECLARE_VISIT \
  virtual ReturnType accept( BaseVisitor &V) { \
    V.visit(*this); \
    return 0; \
  } 

namespace oclacc {

class BaseVisitor;

class BaseVisitable
{
  protected:
    BaseVisitable() { }

  public:
    typedef int ReturnType;
    virtual ~BaseVisitable() {};

    BaseVisitable(const BaseVisitable&) = delete;
    BaseVisitable &operator =(const BaseVisitable &) = delete;

    virtual ReturnType accept(BaseVisitor & ) = 0;
};
} //end namespace oclacc

#endif /* BASE_H */
