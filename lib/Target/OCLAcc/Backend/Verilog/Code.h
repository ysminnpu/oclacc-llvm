#ifndef CODE_H
#define CODE_H

#include <sstream>
#include <memory>

#include "../../HW/typedefs.h"
#include "../../Utils.h"

#include "Portmux.h"

#define I(C) std::string((C*2),' ')

namespace oclacc {

std::string moduleDeclHeader(Component &);
std::string moduleDeclFooter(Component &);
std::string moduleBlockWires(Block &);
std::string moduleInstBlock(Block &);


typedef std::shared_ptr<Portmux> portmux_p;

typedef std::map<const port_p, portmux_p> MuxersTy;
typedef MuxersTy::iterator MuxersItTy;
typedef MuxersTy::const_iterator MuxersConstItTy;

MuxersTy Muxers;


std::string moduleDeclHeader(Component &R) {
  std::stringstream S;
  const Component::PortsTy &I = R.getIns();
  const Component::PortsTy &O = R.getOuts();

  S << "module " << R.getName() << "(\n";
  for (Component::PortsConstItTy P = I.begin(), E = I.end(); P != E; ++P) {
    S << I(1) << "input  ";

    unsigned B = (*P)->getBitWidth();
    if (B != 1)
      S << "[" << B-1 << ":0] ";

    S << (*P)->getUniqueName() << ",\n";

    S << I(1) << "input  " << (*P)->getUniqueName() << "_valid,\n";
    S << I(1) << "output  " << (*P)->getUniqueName() << "_ack";

    if (std::next(P) != E)
      S << ",\n";
  }

  if (I.size() != 0) {
    if (O.size() != 0)
      S << ",\n";
    else
      S << "\n";
  }

  for (Component::PortsConstItTy P = O.begin(), E = O.end(); P != E; ++P) {
    S << I(1) << "output ";

    unsigned B = (*P)->getBitWidth();
    if (B != 1)
      S << "[" << B-1 << ":0] ";

    S << (*P)->getUniqueName() << ",\n";

    S << I(1) << "input  " << (*P)->getUniqueName() << "_ack,\n";
    S << I(1) << "output " << (*P)->getUniqueName() << "_valid";

    if (std::next(P) != E)
      S << ",\n";
  }
  S << "\n);\n";

  return S.str();
}

std::string moduleDeclFooter(Component &R) {
  std::stringstream S;
  S << "endmodule " << " // " << R.getUniqueName() << "\n";
  return S.str();
}

std::string moduleBlockWires(Block &R) {
  std::stringstream S;
  const Component::PortsTy &I = R.getIns();
  const Component::PortsTy &O = R.getOuts();

  Component::PortsTy IO;
  IO.insert(IO.end(), I.begin(), I.end());
  IO.insert(IO.end(), O.begin(), O.end());

  S << "// " << R.getUniqueName() << "\n";

  // first create wires for each port used as input and output
  for (Component::PortsConstItTy P = IO.begin(), E = IO.end(); P != E; ++P) {

    // should already be available as port, so skip it
    if (!(*P)->isPipelined()) continue;

    S << "wire ";

    unsigned B = (*P)->getBitWidth();
    if (B != 1)
      S << "[" << B-1 << ":0] ";

    S << (*P)->getUniqueName();

    S << ";\n";

    // Add valid and ack
    S << "wire " << (*P)->getUniqueName() << "_valid;\n";
    S << "wire " << (*P)->getUniqueName() << "_ack;\n";
  }

  return S.str();
}

std::string moduleConnectWires(Block &R) {
  std::stringstream S;
  S << "// Connections for " << R.getUniqueName() << "\n";


  // It is sufficient to walk through all inputs.
  for (port_p P : R.getIns()) {

    // If there are more Inputs to this, there is already a Portmux instance
    // with an output port already named like the input port where it is used,
    // so we can skip it here.
    if (P->getIns().size() > 1)
      continue;

    // ScalarInputs and alike are directly used as input. Skip them as well.
    if (!P->isPipelined())
      continue;

    base_p I = P->getIn(0);

    S << "assign " << P->getUniqueName() << " = " << I->getUniqueName() << ";\n";
    S << "assign " << P->getUniqueName() << "_valid = " << I->getUniqueName() << "_valid;\n";
    S << "assign " << P->getUniqueName() << "_ack = " << I->getUniqueName() << "_ack;\n";
  }

  return S.str();
}

std::string moduleInstBlock(Block &R) {
  std::stringstream S;

  const Component::PortsTy &I = R.getIns();
  const Component::PortsTy &O = R.getOuts();

  Component::PortsTy IO;
  IO.insert(IO.end(), I.begin(), I.end());
  IO.insert(IO.end(), O.begin(), O.end());
  
  // Ports with multiple inputs have to be replaced with an port multiplexer to
  // decide at runtime, which port will be passed through.
  //
  bool printHead = true;
  for (const port_p P : I) {
    if (P->getIns().size() > 1) {
      portmux_p M = std::make_shared<Portmux>(*P);
      if (printHead) {
        S << "// Muxer for " << R.getUniqueName() << "\n";
        printHead = false;
      }
      S << M->instantiate();
      Muxers[P] = M;
    }
  }

  S << R.getName() << " " << R.getUniqueName() << "(\n";

  // Use can use the Port's name as it is unique. We already created wired for
  // each Port and we will connect them afterwards.
  // Input Ports with multiple inputs have to be connected to a multiplexer,
  // which is already generated.

  if (I.size() > 0) {
    S << I(1) << "// In\n";
    for (Component::PortsConstItTy P = I.begin(), E = I.end(); P != E; ++P) {
      S << I(1) << "." << (*P)->getUniqueName() << "(" << (*P)->getUniqueName() << "),\n";
      S << I(1) << "." << (*P)->getUniqueName() << "_valid(" << (*P)->getUniqueName() << "_valid),\n";
      S << I(1) << "." << (*P)->getUniqueName() << "_ack(" << (*P)->getUniqueName() << "_ack)";

      if (std::next(P) != E || O.size() > 0)
        S << ",\n";
    }
  }
  if (O.size() > 0) {
    S << I(1) << "// Out\n";

    for (Component::PortsConstItTy P = O.begin(), E = O.end(); P != E; ++P) {
      S << I(1) << "." << (*P)->getUniqueName() << "(" << (*P)->getUniqueName() << "),\n";
      S << I(1) << "." << (*P)->getUniqueName() << "_valid(" << (*P)->getUniqueName() << "_valid),\n";
      S << I(1) << "." << (*P)->getUniqueName() << "_ack(" << (*P)->getUniqueName() << "_ack)";
      if (std::next(P) != E)
        S << ",\n";
    }
  }

  S << "\n" << ");\n";
  return S.str();
}

} // end ns oclacc

#endif /* CODE_H */
