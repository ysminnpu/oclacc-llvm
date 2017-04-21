#ifndef TOPMODULE_H
#define TOPMODULE_H

#include <sstream>
#include <vector>

#include "HW/typedefs.h"
#include "Utils.h"
#include "Signal.h"
#include "Naming.h"

#define I(C) std::string((C*2),' ')

namespace oclacc {

class OperatorInstances;


/// \brief Base class to implement Components
class VerilogModule {
  public:

  public:
    VerilogModule(Component &);
    virtual ~VerilogModule();

    virtual const std::string declHeader() const = 0;

    const std::string declFooter() const;

  private:
    Component &Comp;
};

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

/// \brief Instantiate multiple Kernel Functions in one Design
class TopModule {
};

/// \brief Implementation of Kernel Function
class KernelModule : public VerilogModule{
  public:
    KernelModule(Kernel &);

    virtual const std::string declHeader() const;

    const std::string declBlockWires() const;

    const std::string instBlocks() const;

  private:
    Kernel &Comp;

};

} // end ns oclacc

#endif /* TOPMODULE_H */
