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
class Mux;

class Verilog : public DFVisitor {
  private:
    typedef DFVisitor super;

    FileTy FS;
  public:
    Verilog();
    ~Verilog();

    int visit(DesignUnit &);
    int visit(Kernel &);
    int visit(Block &);
    int visit(ScalarPort &);
    int visit(StreamPort &);
    int visit(Arith &);
    int visit(FPArith &);
    int visit(Mux &);
};

} // end ns oclacc


#endif /* VERILOG_H */
