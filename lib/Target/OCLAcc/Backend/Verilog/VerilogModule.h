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

    inline const std::string declBlockSignals() const { return BlockSignals.str(); }
    inline const std::string declConstSignals() const { return ConstSignals.str(); }
    inline const std::string declBlockAssignments() const { return ConstSignals.str(); }
    inline const std::string declBlockComponents() const { return BlockComponents.str(); }

    inline std::stringstream &getBlockSignals() { return BlockSignals; }
    inline std::stringstream &getConstSignals() { return ConstSignals; }
    inline std::stringstream &getBlockAssignments() { return BlockAssignments;}
    inline std::stringstream &getBlockComponents() { return BlockComponents;}

    void schedule(const OperatorInstances &);

    unsigned getReadyCycle(const std::string) const;
    inline unsigned getReadyCycle(base_p P) const {
        return getReadyCycle(getOpName(P));
    }

  private:
    Block &Comp;
    unsigned CriticalPath;

    // Components as instantiated by each component
    std::stringstream BlockSignals;
    std::stringstream ConstSignals;
    std::stringstream BlockAssignments;
    std::stringstream BlockComponents;

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
