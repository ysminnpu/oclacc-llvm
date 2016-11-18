#ifndef KERNEL_H
#define KERNEL_H

#include <map>
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
class Component : public Identifiable, public Visitable {
  protected:
    const llvm::Value *IR;

    typedef std::vector<port_p> PortsTy;
    typedef std::vector<streamport_p> StreamsTy;
    typedef std::vector<scalarport_p> ScalarsTy;

    // We often have to be able to look for a specific Value, so we store them
    // in a map AND a list to avoid having to look up every single item's
    // IR pointer when a new one is to be inserted.
    typedef std::map<const llvm::Value *, streamport_p> StreamMapTy;
    typedef std::map<const llvm::Value *, scalarport_p> ScalarMapTy;

    StreamsTy InStreams;
    StreamMapTy InStreamsMap;

    StreamsTy OutStreams;
    StreamMapTy OutStreamsMap;

    ScalarsTy InScalars;
    ScalarMapTy InScalarsMap;

    ScalarsTy OutScalars;
    ScalarMapTy OutScalarsMap;

    typedef std::vector<const_p> ConstantsType;
    ConstantsType ConstVals;

    virtual bool isBlock() = 0;
    virtual bool isKernel() = 0;

  public:
    const llvm::Value * getIR() const { return IR; }
    void setIR(const llvm::Value *P) { IR=P; }

    // InScalars
    void addInScalar(scalarport_p P) { 
      assert(P->getIR() != nullptr);
      InScalarsMap[P->getIR()] = P; 
      InScalars.push_back(P);
    }

    const ScalarsTy &getInScalars() const { 
      return InScalars; 
    }
    
    bool containsInScalarForValue(const Value *V) {
      ScalarMapTy::const_iterator IT = InScalarsMap.find(V);
      return IT != InScalarsMap.end();
    }

    scalarport_p getInScalarForValue(const Value *V) {
      ScalarMapTy::const_iterator IT = InScalarsMap.find(V);
      if (IT != InScalarsMap.end())
        return IT->second;
      else return nullptr;
    }

    // InStreams
    void addInStream(streamport_p P) { 
      assert(P->getIR() != nullptr);
      InStreamsMap[P->getIR()] = P;
      InStreams.push_back(P);
    }

    bool containsInStreamForValue(const Value *V) {
      StreamMapTy::const_iterator IT = InStreamsMap.find(V);
      return IT != InStreamsMap.end();
    }

    const StreamsTy getInStreams() const { 
      return InStreams; 
    }

    // OutScalars
    void addOutScalar(scalarport_p P) { 
      assert(P->getIR() != nullptr);
      OutScalarsMap[P->getIR()] = P;
      OutScalars.push_back(P);
    }

    const ScalarsTy &getOutScalars() const { 
      return OutScalars;
    }

    bool containsOutScalarForValue(const Value *V) {
      ScalarMapTy::const_iterator IT = OutScalarsMap.find(V);
      return IT != OutScalarsMap.end();
    }

    scalarport_p getOutScalarForValue(const Value *V) {
      ScalarMapTy::const_iterator IT = OutScalarsMap.find(V);
      if (IT != OutScalarsMap.end())
        return IT->second;
      else return nullptr;
    }

    // OutStreams
    void addOutStream(streamport_p P) { 
      assert(P->getIR() != nullptr);
      OutStreamsMap[P->getIR()] = P;
      OutStreams.push_back(P);
    }

    bool containsOutStreamForValue(const Value *V) {
      StreamMapTy::const_iterator IT = OutStreamsMap.find(V);
      return IT != OutStreamsMap.end();
    }

    const StreamsTy &getOutStreams() const { 
      return OutStreams;
    }

    PortsTy getOuts(void) const {
      PortsTy Outs;
      Outs.insert(Outs.end(), OutStreams.begin(), OutStreams.end());
      Outs.insert(Outs.end(), OutScalars.begin(), OutScalars.end());
      return Outs;
    }

    PortsTy getIns(void) const {
      PortsTy Ins;
      Ins.insert(Ins.end(), InStreams.begin(), InStreams.end());
      Ins.insert(Ins.end(), InScalars.begin(), InScalars.end());
      return Ins;
    }

    void addConstVal(const_p p) { ConstVals.push_back(p); }
    const ConstantsType &getConstVals() const { return ConstVals; }

    void dump() {
      outs() << "----------------------\n";
      if (isBlock())
        outs() << "Block " << getUniqueName() << "\n";
      else if (isKernel())
        outs() << "Kernel " << getUniqueName() << "\n";
      outs() << "----------------------\n";

      outs() << "InScalars:\n";
      for (const scalarport_p HWP : getInScalars()) {
        outs() << " "<< HWP->getUniqueName() << "\n";
      }
      outs() << "InStreams:\n";
      for (const streamport_p HWP : getInStreams()) {
        outs() << " "<< HWP->getUniqueName() << "\n";
      }
      outs() << "OutScalars:\n";
      for (const scalarport_p HWP : getOutScalars()) {
        outs() << " "<< HWP->getUniqueName() << "\n";
      }
      outs() << "OutStreams:\n";
      for (const streamport_p HWP : getOutStreams()) {
        outs() << " "<< HWP->getUniqueName() << "\n";
      }
      outs() << "----------------------\n";
    }

  DECLARE_VISIT;

  protected: 
    Component(const std::string &Name) : Identifiable(Name) { }
    virtual ~Component() { }

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
