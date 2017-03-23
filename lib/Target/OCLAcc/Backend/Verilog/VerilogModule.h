#ifndef TOPMODULE_H
#define TOPMODULE_H

#include <sstream>
#include <vector>

#include "../../HW/typedefs.h"
#include "../../Utils.h"
#include "Signal.h"

#define I(C) std::string((C*2),' ')

namespace oclacc {

/// \brief Base class to implement Components
class VerilogModule {
  public:

  public:
    VerilogModule(Component &);

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

    const std::string declEnable() const;

  private:
    Block &Comp;

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

    const std::string instBlocks();

  private:
    Kernel &Comp;

};

} // end ns oclacc

#endif /* TOPMODULE_H */
