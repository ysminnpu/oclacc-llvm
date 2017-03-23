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

struct Operator {
  const std::string Name;
  unsigned Cycles;

  Operator(const std::string Name, unsigned Cycles) : Name(Name), Cycles(Cycles) {
  }
};

typedef std::shared_ptr<Operator> op_p;

class OperatorInstances {
  public:
    typedef std::map<std::string, op_p > OpMapTy;
    typedef OpMapTy::const_iterator OpMapConstItTy;

    // Store Operators by name to speed up lookup
    typedef std::unordered_set<std::string> OpsTy;

  private:
    // Map HW.getUniqueName to Operator.Name
    OpMapTy HWOp;

    // Map Operator.Name to op_p
    OpMapTy NameMap;

    // Store all OpInstances with their name, e.g. fmul_8_23_10
    OpsTy Ops;

  public:
    /// \brief Add Operator. Creates a new if required or just adds another
    /// mapping for HWName
    void addOperator(const std::string HWName, const std::string OpName, unsigned Cycles);

    /// \brief Get existing Operator by Operator's name
    op_p getOperator(const std::string OpName);

    bool existsOperator(const std::string OpName);

    // Lookup HWNames
    bool existsOperatorForHW(const std::string HWName);
    op_p getOperatorForHW(const std::string HWName);

};


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

    // Components as instantiated by each component
    std::stringstream BlockSignals;
    std::stringstream BlockComponents;

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

    void runAsapScheduler(const Block &);
};

} // end ns oclacc

#endif /* VERILOG_H */
