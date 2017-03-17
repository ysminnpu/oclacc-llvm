#ifndef VERILOG_H
#define VERILOG_H

#include <sstream>
#include <list>
#include <memory>

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

// Quick and dirty set various options for flopoco instances

namespace conf {
const bool FPAdd_DualPath = true;

const int IntAdder_Arch = -1;
const int IntAdder_OptObjective = 2;
const bool IntAdder_SRL = true;

const std::string to_string(bool B);
} // end ns conf

struct OpInstance {
  unsigned cycles;
};

typedef std::shared_ptr<OpInstance> op_p;

struct DesignFiles {
  public:
    typedef std::list<std::string> FileListTy;
    typedef std::list<std::string>::const_iterator FileListConstItTy;

  private:
    FileListTy Files;
    // Arithmetic Operators to be generated with Flopoco

  public:
    void addFile(const std::string);

    void write(const std::string Filename);
};

class Verilog : public DFVisitor {
  private:
    typedef DFVisitor super;

    // name:Operator
    typedef std::map<const std::string, op_p > OpMapTy;
    typedef OpMapTy::const_iterator  OpMapConstItTy;

    // Store all OpInstances with their name, e.g. fmul_8_23_10
    OpMapTy Ops;
    // Map HW objects to OpInStances
    OpMapTy HWMap;

    // Flopoco Instances
    std::stringstream FInst;


  public:
    Verilog();
    ~Verilog();

    int visit(DesignUnit &);
    int visit(Kernel &);
    int visit(Block &);

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
};

} // end ns oclacc

#endif /* VERILOG_H */
