#ifndef FLOPOCOFPFORMAT_H
#define FLOPOCOFPFORMAT_H

#include "HW/Visitor/DFVisitor.h"
#include "HW/Writeable.h"
#include "HW/Identifiable.h"
#include "Utils.h"

namespace oclacc {

class ScalarPort;
class FPArith;
class FPCompare;

class FlopocoFPFormat : public DFVisitor {
  private:
    typedef DFVisitor super;

  public:
    FlopocoFPFormat();
    ~FlopocoFPFormat();

    int visit(ScalarPort &);
    int visit(StreamPort &);
    int visit(StreamIndex &);

    // Arith
    int visit(FPArith &);
    int visit(FPCompare &);
};

} // end ns oclacc

#endif /* FLOPOCOFPFORMAT_H */
