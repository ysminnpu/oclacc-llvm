#ifndef BLOCKMODULE_H
#define BLOCKMODULE_H

#include "HW/HW.h"
#include "HW/typedefs.h"
#include "VerilogModule.h"

namespace oclacc {

class Block;
class OperatorInstances;

/// \brief Implementation of Block
class BlockModule : public VerilogModule {
  public:
    BlockModule(Block &);

    virtual const std::string declHeader() const;

    //const std::string declEnable() const;

    const std::string declConstValues() const;

    const std::string declPortControlSignals() const;

    const std::string declFSMSignals() const;

    const std::string declFSM() const;

    const std::string declStores() const;
    const std::string declLoads() const;

    inline const std::string declBlockSignals() const { return BlockSignals.str(); }
    inline const std::string declConstSignals() const { return ConstSignals.str(); }
    inline const std::string declBlockAssignments() const { return BlockAssignments.str(); }
    inline const std::string declLocalOperators() const { return LocalOperators.str(); }
    inline const std::string declBlockComponents() const { return BlockComponents.str(); }

    inline std::stringstream &getBlockSignals() { return BlockSignals; }
    inline std::stringstream &getConstSignals() { return ConstSignals; }
    inline std::stringstream &getBlockAssignments() { return BlockAssignments;}
    inline std::stringstream &getLocalOperators() { return LocalOperators;}
    inline std::stringstream &getBlockComponents() { return BlockComponents;}

    /// \brief Assign each component a clock cycle when all inputs are ready.
    ///
    void schedule(const OperatorInstances &);

    int getReadyCycle(const std::string) const;
    inline int getReadyCycle(base_p P) const {
      return getReadyCycle(P->getUniqueName());
    }

    virtual void genTestBench() const;

  private:
    Block &Comp;
    unsigned CriticalPath;

    // Components as instantiated by each component
    std::stringstream BlockSignals;
    std::stringstream ConstSignals;
    std::stringstream BlockAssignments;
    std::stringstream LocalOperators;
    std::stringstream BlockComponents;

    // Scheduling information
    typedef std::map<std::string, unsigned> ReadyMapTy;
    typedef std::pair<std::string, unsigned> ReadyMapElemTy;
    typedef ReadyMapTy::const_iterator ReadyMapConstItTy;
    ReadyMapTy ReadyMap;
};

} // end ns oclacc

#endif /* BLOCKMODULE_H */
