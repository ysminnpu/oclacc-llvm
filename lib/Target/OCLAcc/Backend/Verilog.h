#ifndef VERILOG_H
#define VERILOG_H

#include "HW/Visitor/DFVisitor.h"

namespace oclacc {

class Verilog : public DFVisitor {
  public:
    Verilog();
};

} // end ns oclacc


#endif /* VERILOG_H */
