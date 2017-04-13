#ifndef VISITABLE_H
#define VISITABLE_H

#include "llvm/Support/Debug.h"
#include "BaseVisitor.h"

//DEBUG_WITH_TYPE("Visitor", llvm::dbgs() << __PRETTY_FUNCTION__ << "\n" );

#define DECLARE_VISIT \
  virtual ReturnType accept( BaseVisitor &V) override { \
    V.visit(*this); \
    return 0; \
  } \

namespace oclacc {

class Visitable
{
  protected:
    Visitable() { }

  public:
    typedef int ReturnType;
    virtual ~Visitable() {};

    Visitable(const Visitable&) = delete;
    Visitable &operator =(const Visitable &) = delete;

    virtual ReturnType accept(BaseVisitor & ) = 0;
};
} //end namespace oclacc

#endif /* VISITABLE_H */
