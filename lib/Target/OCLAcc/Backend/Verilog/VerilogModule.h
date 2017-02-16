#ifndef TOPMODULE_H
#define TOPMODULE_H

#include <sstream>

#include "../../HW/typedefs.h"
#include "../../Utils.h"

#include "Portmux.h"

#define I(C) std::string((C*2),' ')

namespace oclacc {

/// \brief Base class to implement Components
class VerilogModule {
  public:
    VerilogModule(Component &);

    const std::string declHeader() const;

    const std::string declFooter() const;

  private:
    Component &R;

};

/// \brief Implementation of Block
class BlockModule : public VerilogModule {
  public:
    BlockModule(Block &);

    const std::string declEnable() const;

  private:
    Block &R;

};

/// \brief Instantiate multiple Kernel Functions in one Design
class TopModule {
};

/// \brief Implementation of Kernel Function
class KernelModule : public VerilogModule{
  public:
    KernelModule(Kernel &);

    const std::string declBlockWires() const;

    const std::string instBlocks() const;

    const std::string connectWires() const;

    const std::string instStreams() const;

  private:
    Kernel &R;

};

} // end ns oclacc

#endif /* TOPMODULE_H */
