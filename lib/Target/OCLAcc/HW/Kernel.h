#ifndef KERNEL_H
#define KERNEL_H

#include <vector>

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
class Component : public Identifiable {
  protected:
    const llvm::Value *IR;

    std::vector<streamport_p> InStreams;
    std::vector<scalarport_p> InScalars;

    std::vector<streamport_p> OutStreams;
    std::vector<scalarport_p> OutScalars;

    std::vector<const_p> ConstVals;

  public:
    const llvm::Value * getIR() const { return IR; }
    void setIR(const llvm::Value *P) { IR=P; }

    void addInScalar(scalarport_p p) { InScalars.push_back(p); }
    const std::vector<scalarport_p> &getInScalars() const { return InScalars; }

    void addInStream(streamport_p p) { InStreams.push_back(p); }
    const std::vector<streamport_p> &getInStreams() const { return InStreams; }

    void addOutScalar(scalarport_p p) { OutScalars.push_back(p); }
    const std::vector<scalarport_p> &getOutScalars() const { return OutScalars; }

    const std::vector<port_p> getOuts() {
      std::vector<port_p> Outs;
      Outs.insert(Outs.end(), OutStreams.begin(), OutStreams.end());
      Outs.insert(Outs.end(), OutScalars.begin(), OutScalars.end());
      return Outs;
    }

    const std::vector<port_p> getIns() {
      std::vector<port_p> Ins;
      Ins.insert(Ins.end(), InStreams.begin(), InStreams.end());
      Ins.insert(Ins.end(), InScalars.begin(), InScalars.end());
      return Ins;
    }

    void addOutStream(streamport_p p) { OutStreams.push_back(p); }
    const std::vector<streamport_p> &getOutStreams() const { return OutStreams; }

    void addConstVal(const_p p) { ConstVals.push_back(p); }
    const std::vector<const_p> &getConstVals() const { return ConstVals; }


  protected: 
    Component(const std::string &Name) : Identifiable(Name) { }

};

class Block : public Component, public Visitable {
  private:
    std::vector<base_p> Ops;

  public:
    Block (const std::string &Name) : Component(Name) { }

    NO_COPY_ASSIGN(Block)

    void addOp(base_p p) { Ops.push_back(p); }
    const std::vector<base_p> &getOps() const { return Ops; }

    DECLARE_VISIT
};

/// \brief Schedulable Kernels
/// Kernels are blocks which represent the beginning dataflow of a WorkItem.
class Kernel : public Component, public Visitable {
  private:
    bool WorkItem;

    std::vector<block_p> Blocks;
  public:

    Kernel (const std::string &Name="unnamed", bool WorkItem=false) : Component(Name), WorkItem(false) { }

    NO_COPY_ASSIGN(Kernel)

    void addBlock(block_p p) { Blocks.push_back(p); }
    const std::vector<block_p> &getBlocks() const { return Blocks; }

    void setWorkItem(bool T) { WorkItem = T; }
    bool isWorkItem() const { return WorkItem; }

    DECLARE_VISIT
};

} //ns oclacc

#endif /* KERNEL_H */
