#ifndef KERNEL_H
#define KERNEL_H

#include <vector>

#include "Identifiable.h"
#include "Visitor/Visitor.h"
#include "Visitor/Base.h"

namespace oclacc {

enum ConditionFlag {COND_TRUE, COND_FALSE, COND_NONE};
const std::string ConditionFlagNames[] = {
  "COND_TRUE", "COND_FALSE", "COND_NONE"};

/// \brief Basic component used to build dataflow.
/// Blocks have a list of inputs used inside. Blocks use their inputs when valid
/// and generate outputs. Multiple blocks are connected by connecting their
/// inputs/outputs.
class Block : public Identifiable, public BaseVisitable {
  public:

    typedef std::map<block_p, ConditionFlag> successor_t;

  private:
    std::vector<input_p> Ins;
    std::vector<output_p> Outs;
    std::vector<base_p> Ops;
    std::vector<const_p> Consts;

    base_p Condition;
    successor_t Successors;

    const llvm::Value *IR;

  public:
    Block (const std::string &Name) : Identifiable(Name) { }

    NO_COPY_ASSIGN(Block)

    void addIn(input_p P) { Ins.push_back(P); }
    const std::vector<input_p> &getIns() const { return Ins; }

    void addOut(output_p P) { Outs.push_back(P); }
    const std::vector<output_p> &getOuts() const { return Outs; }

    void addOp(base_p P) { Ops.push_back(P); }
    const std::vector<base_p> &getOps() const { return Ops; }

    void addConst(const_p P) { Consts.push_back(P); }
    const std::vector<const_p> &getConsts() const { return Consts; }

    void setCondition(base_p P) {
      Condition = P;
    }

    const base_p &getCondition() const {
      return Condition;
    }

    void addSuccessor(block_p P, ConditionFlag F) {
      Successors[P] = F;
    }

    const ConditionFlag &getConditionFlag(block_p B) const {
      successor_t::const_iterator It = Successors.find(B);

      if (It == Successors.end()) {
        llvm::errs() << "From " << getName() << "to Block: " << B->getName() << "\n";
        llvm_unreachable("Successor block not in map.");
      }

      return It->second;
    }

    const std::map<block_p, ConditionFlag> &getSuccessors() const {
       return Successors;
    }

    const llvm::Value * getIR() const { return IR; }
    void setIR(const llvm::Value *P) { IR=P; }

    DECLARE_VISIT
};

/// \brief Schedulable Blocks
/// Kernels are blocks which represent the beginning dataflow of a WorkItem.
class Kernel : public Identifiable, public BaseVisitable
{
  private:
    bool WorkItem;
    const llvm::Value *IR;

    std::vector<instream_p> InStreams;
    std::vector<inscalar_p> InScalars;

    std::vector<outstream_p> OutStreams;
    std::vector<outscalar_p> OutScalars;

    std::vector<const_p> ConstVals;
    std::vector<block_p> Blocks;

  public:

    Kernel (const std::string &Name="unnamed", bool WorkItem=false) : Identifiable(Name), WorkItem(false) { }

    NO_COPY_ASSIGN(Kernel)

    const llvm::Value * getIR() const { return IR; }
    void setIR(const llvm::Value *P) { IR=P; }

    void addInScalar(inscalar_p p) { InScalars.push_back(p); }
    const std::vector<inscalar_p> &getInScalars() const { return InScalars; }

    void addInStream(instream_p p) { InStreams.push_back(p); }
    const std::vector<instream_p> &getInStreams() const { return InStreams; }

    void addOutScalar(outscalar_p p) { OutScalars.push_back(p); }
    const std::vector<outscalar_p> &getOutScalars() const { return OutScalars; }

    void addOutStream(outstream_p p) { OutStreams.push_back(p); }
    const std::vector<outstream_p> &getOutStreams() const { return OutStreams; }

    void addConstVal(const_p p) { ConstVals.push_back(p); }
    const std::vector<const_p> &getConstVals() const { return ConstVals; }

    void addBlock(block_p p) { Blocks.push_back(p); }
    const std::vector<block_p> &getBlocks() const { return Blocks; }

    void setWorkItem(bool T) { WorkItem = T; }
    bool isWorkItem() const { return WorkItem; }

    DECLARE_VISIT
};

} //ns oclacc

#endif /* KERNEL_H */
