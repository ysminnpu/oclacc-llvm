#ifndef KERNEL_H
#define KERNEL_H

#include <set>

#include "Identifiable.h"
#include "Visitor/Visitable.h"
#include "Streams.h"

namespace oclacc {

enum ConditionFlag {COND_TRUE, COND_FALSE, COND_NONE};
const std::string ConditionFlagNames[] = {
  "COND_TRUE", "COND_FALSE", "COND_NONE"};

/// \brief Basic component used to build dataflow.
/// Components, i.e. Blocks and Kernels have a list of inputs used inside. Blocks use their inputs when valid
/// and generate outputs. Multiple blocks are connected by connecting their
/// inputs/outputs.
class Component : public Identifiable, public Visitable {
  protected:
    const llvm::Value *IR;

    typedef std::vector<port_p> PortsType;

    typedef std::set<streamport_p> StreamsType;
    StreamsType InStreams;
    StreamsType OutStreams;

    typedef std::set<scalarport_p> ScalarsType;
    ScalarsType InScalars;
    ScalarsType OutScalars;

    typedef std::set<const_p> ConstantsType;
    ConstantsType ConstVals;

    virtual bool isBlock() = 0;
    virtual bool isKernel() = 0;

  public:
    const llvm::Value * getIR() const { return IR; }
    void setIR(const llvm::Value *P) { IR=P; }

    void addInScalar(scalarport_p p) { InScalars.insert(p); }
    const ScalarsType &getInScalars() const { return InScalars; }

    void addInStream(streamport_p p) { InStreams.insert(p); }
    const StreamsType &getInStreams() const { return InStreams; }

    void addOutScalar(scalarport_p p) { OutScalars.insert(p); }
    const ScalarsType &getOutScalars() const { return OutScalars; }

    const PortsType getOuts(void) const {
      PortsType Outs;
      Outs.insert(Outs.end(), OutStreams.begin(), OutStreams.end());
      Outs.insert(Outs.end(), OutScalars.begin(), OutScalars.end());
      return Outs;
    }

    const ScalarsType &getOutScalars(void) {
      return OutScalars;
    }
    const StreamsType &getOutStreams(void) {
      return OutStreams;
    }

    const PortsType getIns(void) const {
      PortsType Ins;
      Ins.insert(Ins.end(), InStreams.begin(), InStreams.end());
      Ins.insert(Ins.end(), InScalars.begin(), InScalars.end());
      return Ins;
    }

    const ScalarsType &getInScalars(void) {
      return InScalars;
    }
    const StreamsType &getInStreams(void) {
      return InStreams;
    }

    void addOutStream(streamport_p p) { OutStreams.insert(p); }
    const StreamsType &getOutStreams() const { return OutStreams; }

    void addConstVal(const_p p) { ConstVals.insert(p); }
    const ConstantsType &getConstVals() const { return ConstVals; }

    void dump() {
      outs() << "----------------------\n";
      if (isBlock())
        outs() << "Block " << Name << "\n";
      else if (isKernel())
        outs() << "Kernel " << Name << "\n";
      outs() << "----------------------\n";

      outs() << "InScalars:\n";
      for (const scalarport_p HWP : getInScalars()) {
        outs() << " "<< HWP->getName() << "\n";
      }
      outs() << "InStreams:\n";
      for (const streamport_p HWP : getInStreams()) {
        outs() << " "<< HWP->getName() << "\n";
      }
      outs() << "OutScalars:\n";
      for (const scalarport_p HWP : getOutScalars()) {
        outs() << " "<< HWP->getName() << "\n";
      }
      outs() << "OutStreams:\n";
      for (const streamport_p HWP : getOutStreams()) {
        outs() << " "<< HWP->getName() << "\n";
      }
      outs() << "----------------------\n";
    }

  DECLARE_VISIT;

  protected: 
    Component(const std::string &Name) : Identifiable(Name) { }

};

class Block : public Component {
  private:
    std::vector<base_p> Ops;
    kernel_p Parent;

  protected:
    bool isBlock() { return true; }
    bool isKernel() { return false; }

  public:
    Block (const std::string &Name) : Component(Name) { }

    NO_COPY_ASSIGN(Block)

    void addOp(base_p P) { Ops.push_back(P); }
    const std::vector<base_p> &getOps() const { return Ops; }

    kernel_p getParent() const {
      return Parent;
    }

    void setParent(kernel_p P) {
      Parent = P;
    }

    DECLARE_VISIT;
};

/// \brief Schedulable Kernels
/// Kernels are blocks which represent the beginning dataflow of a WorkItem.
class Kernel : public Component {
  private:
    bool WorkItem;

    std::vector<block_p> Blocks;
  protected:
    bool isBlock() { return false; }
    bool isKernel() { return true; }

  public:

    Kernel (const std::string &Name="unnamed", bool WorkItem=false) : Component(Name), WorkItem(false) { }

    NO_COPY_ASSIGN(Kernel)

    void addBlock(block_p p) { Blocks.push_back(p); }
    const std::vector<block_p> &getBlocks() const { return Blocks; }

    void setWorkItem(bool T) { WorkItem = T; }
    bool isWorkItem() const { return WorkItem; }

    DECLARE_VISIT;
};

} //ns oclacc

#endif /* KERNEL_H */
