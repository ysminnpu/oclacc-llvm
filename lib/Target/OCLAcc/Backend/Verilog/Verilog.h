#ifndef VERILOG_H
#define VERILOG_H

#include <sstream>

#include "HW/Visitor/DFVisitor.h"
#include "HW/Writeable.h"
#include "HW/Identifiable.h"
#include "Utils.h"

namespace oclacc {

class DesignUnit;
class Kernel;
class Block;
class ScalarPort;
class StreamPort;
class Arith;
class FPArith;

class Verilog : public DFVisitor {
  private:
    typedef DFVisitor super;

    FileTy FS;
  public:
    Verilog();
    ~Verilog();

    int visit(DesignUnit &R);
    int visit(Kernel &R);
    int visit(Block &R);
    int visit(ScalarPort &R);
    int visit(StreamPort &R);
    int visit(Arith &R);
    int visit(FPArith &R);
};

} // end ns oclacc


#endif /* VERILOG_H */
