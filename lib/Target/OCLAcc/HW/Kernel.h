#ifndef KERNEL_H
#define KERNEL_H

#include <map>
#include <vector>

#include "typedefs.h"
#include "Identifiable.h"
#include "Visitor/Visitable.h"
#include "Port.h"

namespace oclacc {

enum ConditionFlag {COND_TRUE, COND_FALSE, COND_NONE};
const std::string ConditionFlagNames[] = {
  "COND_TRUE", "COND_FALSE", "COND_NONE"};

/// \brief Basic component used to build dataflow.
/// Components, i.e. Blocks and Kernels have a list of inputs used inside. Blocks use their inputs when valid
/// and generate outputs. Multiple blocks are connected by connecting their
/// inputs/outputs.
class Component : public Identifiable, public Visitable {
  public:
    typedef std::vector<port_p> PortsTy;
    typedef std::vector<port_p>::iterator PortsItTy;
    typedef std::vector<port_p>::const_iterator PortsConstItTy;

    typedef std::vector<scalarport_p> ScalarsTy;

  protected:
    const llvm::Value *IR;


    // We often have to be able to look for a specific Value, so we store them
    // in a map AND a list to avoid having to look up every single item's
    // IR pointer when a new one is to be inserted.
    typedef std::map<const llvm::Value *, scalarport_p> ScalarMapTy;

    ScalarsTy InScalars;
    ScalarMapTy InScalarsMap;

    ScalarsTy OutScalars;
    ScalarMapTy OutScalarsMap;

    typedef std::vector<const_p> ConstantsType;
    ConstantsType ConstVals;

  public:
    const llvm::Value * getIR() const;
    void setIR(const llvm::Value *);

    virtual bool isBlock() = 0;
    virtual bool isKernel() = 0;

    // InScalars
    void addInScalar(scalarport_p);

    const ScalarsTy &getInScalars() const;
    
    bool containsInScalarForValue(const Value *);

    scalarport_p getInScalarForValue(const Value *);

    // OutScalars
    void addOutScalar(scalarport_p);

    const ScalarsTy &getOutScalars() const;

    bool containsOutScalarForValue(const Value *V);

    scalarport_p getOutScalarForValue(const Value *V);

    void addConstVal(const_p p);
    const ConstantsType &getConstVals() const;

    virtual void dump() = 0;

  protected: 
    Component(const std::string &);
    virtual ~Component();

};

class Block : public Component {
  public:
    typedef std::pair<base_p, block_p> CondTy;
    typedef std::vector<CondTy> CondListTy;
    typedef CondListTy::iterator CondItTy;
    typedef CondListTy::const_iterator CondConstItTy;

    typedef std::vector<streamindex_p> StreamIndicesTy;
    typedef std::vector<staticstreamindex_p> StaticStreamIndicesTy;
    typedef std::vector<dynamicstreamindex_p> DynamicStreamIndicesTy;
    typedef std::map<const llvm::Value *, streamindex_p> StreamIndexMapTy;

    // TrueFalse Type
    typedef std::pair<base_p, base_p> TFTy;
    typedef std::map<scalarport_p, TFTy> SingleCondTy;

  private:
    std::vector<base_p> Ops;
    kernel_p Parent;

    CondListTy Conds;
    CondListTy NegConds;

    StreamIndicesTy InStreamIndices;
    StreamIndexMapTy InStreamIndicesMap;

    StreamIndicesTy OutStreamIndices;
    StreamIndexMapTy OutStreamIndicesMap;

    bool EntryBlock;

  public:
    Block (const std::string &, bool);

    NO_COPY_ASSIGN(Block)

    void addOp(base_p);
    const std::vector<base_p> &getOps() const;

    inline kernel_p getParent() const {
      return Parent;
    }

    inline void setParent(kernel_p P) {
      Parent = P;
    }

    /// \brief If \p P is true, this Block is the successor of \p B
    inline void addCond(base_p P, block_p B) {
      Conds.push_back(std::make_pair(P, B));
    }

    /// \brief If \p P is false, this Block is the successor of \p B
    inline void addNegCond(base_p P, block_p B) {
      NegConds.push_back(std::make_pair(P, B));
    }

    inline const CondListTy& getConds() const {
      return Conds;
    }
    inline const CondListTy& getNegConds() const {
      return NegConds;
    }

    virtual inline bool isBlock() override {
      return true;
    }
    virtual inline bool isKernel() override {
      return false;
    }

    inline bool isConditional() const {
      return !(getConds().empty() && getNegConds().empty());
    }

    const scalarport_p getCondReachedByBlock(block_p) const;
    const scalarport_p getNegCondReachedByBlock(block_p) const;

    /// \brief Return Condition for a Scalar Input Port. Only makes sense if
    /// ScalarPort has multiple inputs.
    const SingleCondTy getCondForScalarPort(scalarport_p) const;

    inline bool isEntryBlock() const {
      return EntryBlock;
    }

    // InStreams
    void addInStreamIndex(streamindex_p);

    bool containsInStreamIndexForValue(const Value *);

    const StreamIndicesTy &getInStreamIndices() const;
    const StaticStreamIndicesTy &getStaticInStreamIndices() const;
    const DynamicStreamIndicesTy &getDynamicInStreamIndices() const;

    // OutStreams
    void addOutStreamIndex(streamindex_p);

    bool containsOutStreamIndexForValue(const Value *);

    const StreamIndicesTy &getOutStreamIndices() const;
    const StaticStreamIndicesTy &getStaticOutStreamIndices() const;
    const DynamicStreamIndicesTy &getDynamicOutStreamIndices() const;
    
    virtual void dump();

    DECLARE_VISIT;
};

/// \brief Schedulable Kernels
/// Kernels are blocks which represent the beginning dataflow of a WorkItem.
class Kernel : public Component {
  public:
    typedef std::vector<streamport_p> StreamsTy;
    typedef std::map<const llvm::Value *, streamport_p> StreamMapTy;
    typedef std::vector<block_p> BlocksTy;

  private:

  private:
    bool WorkItem;

    BlocksTy Blocks;

    StreamsTy Streams;
    StreamMapTy StreamsMap;
#if 0
    StreamsTy InStreams;
    StreamMapTy InStreamsMap;

    StreamsTy OutStreams;
    StreamMapTy OutStreamsMap;

    StreamsTy InOutStreams;
    StreamMapTy InOutStreamsMap;
#endif

  public:

    Kernel (const std::string &, bool);

    NO_COPY_ASSIGN(Kernel)

    virtual inline bool isBlock() override {
      return false;
    }
    virtual inline bool isKernel() override {
      return true;
    }

    void addBlock(block_p p);
    const BlocksTy &getBlocks() const;

    void setWorkItem(bool T);
    bool isWorkItem() const;

    void addStream(streamport_p);
    const StreamsTy getStreams() const;

#if 0

    // InStreams
    void addInStream(streamport_p);

    bool containsInStreamForValue(const Value *);

    const StreamsTy getInStreams() const;

    // OutStreams
    void addOutStream(streamport_p);

    bool containsOutStreamForValue(const Value *);

    const StreamsTy &getOutStreams() const;
    
    // InOutStreams
    void addInOutStream(streamport_p);

    bool containsInOutStreamForValue(const Value *);
    
    const StreamsTy &getInOutStreams() const;
#endif

    // Unified access
    virtual const PortsTy getOuts(void) const;

    virtual const PortsTy getIns(void) const;

    const PortsTy getPorts(void) const;

    virtual void dump();

    DECLARE_VISIT;
};

} //ns oclacc

#endif /* KERNEL_H */
