#ifndef VERILOG_H
#define VERILOG_H

#include <sstream>
#include <list>
#include <memory>
#include <unordered_set>

#include "HW/Visitor/DFVisitor.h"
#include "HW/Writeable.h"
#include "HW/Identifiable.h"
#include "Utils.h"

namespace oclacc {

class BlockModule;
class KernelModule;

// Quick and dirty set various options for flopoco instances
namespace conf {
const bool FPAdd_DualPath = true;

const int IntAdder_Arch = -1;
const int IntAdder_OptObjective = 2;
const bool IntAdder_SRL = true;

const std::string to_string(bool B);
} // end ns conf


class Verilog : public DFVisitor {
  private:
    typedef DFVisitor super;

    std::unique_ptr<KernelModule> KM;
    std::unique_ptr<BlockModule> BM;

    unsigned II=0;

    void handleInferableMath(const Arith &R, const std::string Op);


  public:
    Verilog();
    ~Verilog();

    int visit(DesignUnit &);
    int visit(Kernel &);
    int visit(Block &);

    // Ports
    int visit(ScalarPort &);
    int visit(StreamPort &);
    int visit(LoadAccess &);
    int visit(StoreAccess &);
    int visit(StaticStreamIndex &);
    int visit(DynamicStreamIndex &);

    // Arith
    int visit(Add &); 
    int visit(FAdd &);
    int visit(Sub &);
    int visit(FSub&);
    int visit(Mul &);
    int visit(FMul &);
    int visit(UDiv &);
    int visit(SDiv &);
    int visit(FDiv &);
    int visit(URem &);
    int visit(SRem &);
    int visit(FRem &);
    int visit(Shl &);
    int visit(LShr &);
    int visit(AShr &);
    int visit(And &);
    int visit(Or &);
    int visit(Xor &);

    // Compare
    int visit(IntCompare &);
    int visit(FPCompare &);

    int visit(Mux &);

    int visit(Barrier &);
};

} // end ns oclacc

#endif /* VERILOG_H */
