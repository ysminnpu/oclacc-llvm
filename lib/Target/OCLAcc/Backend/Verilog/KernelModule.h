#ifndef KERNELMODULE_H
#define KERNELMODULE_H

#include "VerilogModule.h"

namespace oclacc {

class Kernel;

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

#endif /* KERNELMODULE_H */
