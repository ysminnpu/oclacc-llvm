#ifndef TOPMODULE_H
#define TOPMODULE_H

#include <sstream>
#include <vector>

#include "../../HW/typedefs.h"
#include "../../Utils.h"

#define I(C) std::string((C*2),' ')

namespace oclacc {

enum SignalDirection {
  In,
  Out
};
const std::string SignalDirection_S[] = {"input", "output"};

enum SignalType {
  Wire,
  Reg
};
const std::string SignalType_S[] = {"wire", "reg"};

struct Signal {
  std::string Name;
  unsigned BitWidth;
  SignalDirection Direction;
  SignalType Type;

  Signal(std::string, unsigned, SignalDirection, SignalType);

  const std::string getDirectionStr(void) const;
  const std::string getTypeStr(void) const;
};

typedef std::vector<Signal> PortListTy;
const PortListTy getInScalarPortSignals(const ScalarPort &);
const PortListTy getInStreamPortSignals(const StreamPort &);
const PortListTy getOutScalarPortSignals(const ScalarPort &);
const PortListTy getOutStreamPortSignals(const StreamPort &);

const PortListTy getInPortSignals(const port_p);
const PortListTy getOutPortSignals(const port_p);

const PortListTy getComponentSignals(const component_p);
const PortListTy getComponentSignals(const Component &);


/// \brief Base class to implement Components
class VerilogModule {
  public:
    // cannot store const strings
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

    const std::string instBlocks();

    const std::string connectWires() const;

    const std::string instStreams() const;



  private:
    Kernel &R;

};

} // end ns oclacc

#endif /* TOPMODULE_H */
